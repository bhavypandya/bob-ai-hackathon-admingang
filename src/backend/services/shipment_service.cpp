#include "shipment_service.h"
#include <algorithm>
#include <sstream>

namespace services {

ShipmentService::ShipmentService(db::Database& db) : db_(db) {}

models::Shipment ShipmentService::rowToShipment(int /*col*/, const char** vals, const char** /*names*/) {
    models::Shipment s;
    s.id = vals[0] ? vals[0] : "";
    s.origin = vals[1] ? vals[1] : "";
    s.destination = vals[2] ? vals[2] : "";
    s.current_route = vals[3] ? vals[3] : "";
    s.carrier = vals[4] ? vals[4] : "";
    s.fleet_asset = vals[5] ? vals[5] : "";

    std::string st = vals[6] ? vals[6] : "";
    if (st == "DELAYED") s.status = models::ShipmentStatus::DELAYED;
    else if (st == "AT_RISK") s.status = models::ShipmentStatus::AT_RISK;
    else if (st == "DISRUPTED") s.status = models::ShipmentStatus::DISRUPTED;
    else if (st == "DELIVERED") s.status = models::ShipmentStatus::DELIVERED;
    else s.status = models::ShipmentStatus::ON_TIME;

    std::string prio = vals[7] ? vals[7] : "";
    if (prio == "CRITICAL") s.priority = models::ShipmentPriority::CRITICAL;
    else if (prio == "HIGH") s.priority = models::ShipmentPriority::HIGH;
    else if (prio == "MEDIUM") s.priority = models::ShipmentPriority::MEDIUM;
    else s.priority = models::ShipmentPriority::LOW;

    s.cargo_type = vals[8] ? vals[8] : "";
    s.cold_chain = vals[9] && std::string(vals[9]) == "1";
    s.planned_departure = vals[10] ? vals[10] : "";
    s.planned_arrival = vals[11] ? vals[11] : "";
    s.current_eta = vals[12] ? vals[12] : "";
    s.expected_delay_hours = vals[13] ? std::stoi(vals[13]) : 0;

    // Defaults for computed fields
    s.impact_score = 0;
    s.is_affected = false;
    s.risk_level = "LOW";

    return s;
}

std::vector<models::Shipment> ShipmentService::getAllShipments() {
    std::vector<models::Shipment> result;
    db_.query("SELECT * FROM shipments ORDER BY id",
        [&](int c, const char** v, const char** n) {
            result.push_back(rowToShipment(c, v, n));
        });
    return result;
}

models::Shipment ShipmentService::getById(const std::string& id) {
    models::Shipment result{};
    result.id = "";
    db_.query("SELECT * FROM shipments WHERE id='" + id + "'",
        [&](int c, const char** v, const char** n) {
            result = rowToShipment(c, v, n);
        });
    return result;
}

// ── Impact Score Computation ──────────────────────────────────────────────────
int ShipmentService::computeImpactScore(const models::Shipment& s,
                                        const std::vector<models::Disruption>& disruptions,
                                        std::vector<std::string>& reasons) {
    int score = 0;

    // Check carrier disruption once (outside disruption loop)
    bool carrier_affected = false;
    db_.query("SELECT is_disrupted FROM carriers WHERE id='" + s.carrier + "'",
        [&](int, const char** v, const char**) {
            carrier_affected = v[0] && std::string(v[0]) == "1";
        });
    if (carrier_affected) {
        score += 20;
        reasons.push_back("Carrier " + s.carrier + " is disrupted");
    }

    for (const auto& d : disruptions) {
        if (d.status == models::DisruptionStatus::RESOLVED) continue;

        bool route_affected = false;
        for (const auto& r : d.affected_routes) {
            if (r == s.current_route) { route_affected = true; break; }
        }

        if (route_affected) {
            score += 30;
            reasons.push_back("Current route (" + s.current_route + ") is affected by " + d.name);
        }
    }

    // Delay severity
    if (s.expected_delay_hours >= 48) {
        score += 20;
        reasons.push_back("Expected delay is severe (" + std::to_string(s.expected_delay_hours) + " hours)");
    } else if (s.expected_delay_hours >= 24) {
        score += 12;
        reasons.push_back("Expected delay is significant (" + std::to_string(s.expected_delay_hours) + " hours)");
    } else if (s.expected_delay_hours >= 6) {
        score += 6;
        reasons.push_back("Expected delay of " + std::to_string(s.expected_delay_hours) + " hours");
    }

    // Priority modifier
    if (s.priority == models::ShipmentPriority::CRITICAL) {
        score += 15;
        reasons.push_back("Shipment has CRITICAL delivery priority");
    } else if (s.priority == models::ShipmentPriority::HIGH) {
        score += 10;
        reasons.push_back("Shipment has HIGH delivery priority");
    }

    // Cold-chain vulnerability
    if (s.cold_chain) {
        score += 10;
        reasons.push_back("Cold-chain shipment — temperature risk during extended delay");
    }

    // Status-based bump
    if (s.status == models::ShipmentStatus::DISRUPTED) {
        score += 5;
    }

    if (score > 100) score = 100;
    return score;
}

std::vector<models::Shipment> ShipmentService::getAffectedShipments(
    const std::vector<models::Disruption>& active_disruptions) {

    auto all = getAllShipments();
    std::vector<models::Shipment> affected;

    // Collect all disrupted route IDs across active disruptions
    std::vector<std::string> disrupted_routes;
    for (const auto& d : active_disruptions) {
        if (d.status == models::DisruptionStatus::RESOLVED) continue;
        for (const auto& r : d.affected_routes)
            disrupted_routes.push_back(r);
    }

    // Also collect disrupted carrier IDs from DB
    std::vector<std::string> disrupted_carriers;
    db_.query("SELECT id FROM carriers WHERE is_disrupted=1",
        [&](int, const char** v, const char**) {
            if (v[0]) disrupted_carriers.push_back(v[0]);
        });

    for (auto& s : all) {
        if (s.status == models::ShipmentStatus::DELIVERED) continue;

        bool route_hit = std::find(disrupted_routes.begin(), disrupted_routes.end(), s.current_route) != disrupted_routes.end();
        bool carrier_hit = std::find(disrupted_carriers.begin(), disrupted_carriers.end(), s.carrier) != disrupted_carriers.end();

        if (route_hit || carrier_hit || s.expected_delay_hours > 0) {
            std::vector<std::string> reasons;
            int score = computeImpactScore(s, active_disruptions, reasons);
            if (score > 0) {
                s.is_affected = true;
                s.impact_score = score;
                s.risk_level = models::Shipment::riskLabel(score);
                s.impact_reasons = reasons;
                affected.push_back(s);
            }
        }
    }

    // Sort by impact score descending
    std::sort(affected.begin(), affected.end(),
        [](const models::Shipment& a, const models::Shipment& b) {
            return a.impact_score > b.impact_score;
        });

    return affected;
}

void ShipmentService::updateStatus(const std::string& id, const std::string& status, int delay_hours) {
    db_.exec("UPDATE shipments SET status='" + status + "', expected_delay_hours=" + std::to_string(delay_hours)
        + " WHERE id='" + id + "'");
}

} // namespace services
