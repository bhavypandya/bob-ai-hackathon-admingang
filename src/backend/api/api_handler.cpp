#include "api_handler.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <cctype>

using json = nlohmann::json;

namespace api {

// ── Helpers ───────────────────────────────────────────────────────────────────
static json disruptionToJson(const models::Disruption& d) {
    json j;
    j["id"] = d.id;
    j["type"] = models::Disruption::typeToString(d.type);
    j["name"] = d.name;
    j["location"] = d.location;
    j["severity"] = models::Disruption::severityToString(d.severity);
    j["start_time"] = d.start_time;
    j["expected_end_time"] = d.expected_end_time;
    j["affected_routes"] = d.affected_routes;
    j["description"] = d.description;
    j["status"] = models::Disruption::statusToString(d.status);
    return j;
}

static json shipmentToJson(const models::Shipment& s) {
    json j;
    j["id"] = s.id;
    j["origin"] = s.origin;
    j["destination"] = s.destination;
    j["current_route"] = s.current_route;
    j["carrier"] = s.carrier;
    j["fleet_asset"] = s.fleet_asset;
    j["status"] = models::Shipment::statusToString(s.status);
    j["priority"] = models::Shipment::priorityToString(s.priority);
    j["cargo_type"] = s.cargo_type;
    j["cold_chain"] = s.cold_chain;
    j["planned_departure"] = s.planned_departure;
    j["planned_arrival"] = s.planned_arrival;
    j["current_eta"] = s.current_eta;
    j["expected_delay_hours"] = s.expected_delay_hours;
    j["impact_score"] = s.impact_score;
    j["is_affected"] = s.is_affected;
    j["risk_level"] = s.risk_level;
    j["impact_reasons"] = s.impact_reasons;
    return j;
}

static json routeToJson(const models::Route& r) {
    json j;
    j["id"] = r.id;
    j["name"] = r.name;
    j["origin"] = r.origin;
    j["destination"] = r.destination;
    j["estimated_hours"] = r.estimated_hours;
    j["total_distance_km"] = r.total_distance_km;
    j["cost_usd"] = r.cost_usd;
    j["capacity_kg"] = r.capacity_kg;
    j["is_disrupted"] = r.is_disrupted;
    j["disruption_reason"] = r.disruption_reason;
    j["risk_score"] = r.risk_score;
    j["refrigerated_capable"] = r.refrigerated_capable;
    return j;
}

static json carrierToJson(const models::Carrier& c) {
    json j;
    j["id"] = c.id;
    j["name"] = c.name;
    j["type"] = c.type;
    j["available_capacity_kg"] = c.available_capacity_kg;
    j["total_capacity_kg"] = c.total_capacity_kg;
    j["reliability_score"] = c.reliability_score;
    j["cost_per_km"] = c.cost_per_km;
    j["avg_delay_hours"] = c.avg_delay_hours;
    j["cold_chain_capable"] = c.cold_chain_capable;
    j["is_disrupted"] = c.is_disrupted;
    j["disruption_reason"] = c.disruption_reason;
    j["service_region"] = c.service_region;
    return j;
}

static json assetToJson(const models::FleetAsset& a) {
    json j;
    j["id"] = a.id;
    j["type"] = models::FleetAsset::typeToString(a.type);
    j["location"] = a.location;
    j["status"] = models::FleetAsset::statusToString(a.status);
    j["capacity_kg"] = a.capacity_kg;
    j["current_assignment"] = a.current_assignment;
    j["availability_time"] = a.availability_time;
    j["utilisation_pct"] = a.utilisation_pct;
    j["refrigerated"] = a.refrigerated;
    return j;
}

static json alertToJson(const models::ColdChainAlert& a) {
    json j;
    j["shipment_id"] = a.shipment_id;
    j["current_temp"] = a.current_temp;
    j["peak_temp"] = a.peak_temp;
    j["min_temp_recorded"] = a.min_temp_recorded;
    j["max_temp_recorded"] = a.max_temp_recorded;
    j["excursion_duration_minutes"] = a.excursion_duration_minutes;
    j["excursion_count"] = a.excursion_count;
    j["minutes_to_delivery"] = a.minutes_to_delivery;
    j["severity"] = models::ColdChainAlert::severityToString(a.severity);
    j["severity_reason"] = a.severity_reason;
    j["recommended_action"] = a.recommended_action;
    j["config"]["min_temp"] = a.config.min_temp;
    j["config"]["max_temp"] = a.config.max_temp;
    j["config"]["warning_margin"] = a.config.warning_margin;
    j["config"]["critical_margin"] = a.config.critical_margin;
    j["config"]["allowed_excursion_minutes"] = a.config.allowed_excursion_minutes;
    return j;
}

static json recToJson(const models::Recommendation& r) {
    json j;
    j["id"] = r.id;
    j["type"] = models::Recommendation::typeToString(r.type);
    j["priority"] = models::Recommendation::priorityToString(r.priority);
    j["target"] = r.target;
    j["reason"] = r.reason;
    j["expected_impact"] = r.expected_impact;
    j["action"] = r.action;
    return j;
}

// ── Handlers ──────────────────────────────────────────────────────────────────
std::string handleHealth() {
    json j;
    j["status"] = "ok";
    j["service"] = "Supply Chain Disruption Assistant";
    j["version"] = "1.0.0";
    return j.dump();
}

std::string handleDashboard(
    db::Database& /*db*/,
    services::DisruptionService& dis,
    services::ShipmentService& ship,
    services::FleetService& fleet,
    services::ColdChainService& cold,
    services::RecommendationService& rec_svc) {

    auto disruptions = dis.getActiveDisruptions();
    auto affected = ship.getAffectedShipments(disruptions);
    auto util = fleet.getUtilisation();
    auto idle = fleet.getIdleAssets();
    auto cold_alerts = cold.getAllAlerts();
    auto recs = rec_svc.generateAll();

    int cold_issues = 0;
    for (const auto& a : cold_alerts)
        if (a.severity != models::ExcursionSeverity::NORMAL) cold_issues++;

    int high_critical = 0;
    for (const auto& s : affected)
        if (s.impact_score >= 50) high_critical++;

    json j;
    j["kpi"]["active_disruptions"] = disruptions.size();
    j["kpi"]["affected_shipments"] = affected.size();
    j["kpi"]["fleet_utilisation_pct"] = static_cast<int>(util.average_utilisation_pct);
    j["kpi"]["idle_assets"] = util.idle_assets;
    j["kpi"]["cold_chain_alerts"] = cold_issues;
    j["kpi"]["high_critical_risks"] = high_critical;

    json dis_arr = json::array();
    for (const auto& d : disruptions) dis_arr.push_back(disruptionToJson(d));
    j["disruptions"] = dis_arr;

    json sh_arr = json::array();
    for (const auto& s : affected) sh_arr.push_back(shipmentToJson(s));
    j["affected_shipments"] = sh_arr;

    json fl_arr = json::array();
    for (const auto& a : idle) fl_arr.push_back(assetToJson(a));
    j["idle_fleet"] = fl_arr;

    json cc_arr = json::array();
    for (const auto& a : cold_alerts)
        if (a.severity != models::ExcursionSeverity::NORMAL)
            cc_arr.push_back(alertToJson(a));
    j["cold_chain_alerts"] = cc_arr;

    json rec_arr = json::array();
    for (const auto& r : recs) rec_arr.push_back(recToJson(r));
    j["recommendations"] = rec_arr;

    j["fleet_utilisation"]["total"] = util.total_assets;
    j["fleet_utilisation"]["active"] = util.active_assets;
    j["fleet_utilisation"]["idle"] = util.idle_assets;
    j["fleet_utilisation"]["average_pct"] = static_cast<int>(util.average_utilisation_pct);
    j["fleet_utilisation"]["redeployment_opportunities"] = util.redeployment_opportunities;

    return j.dump();
}

std::string handleGetShipments(services::ShipmentService& ship) {
    json arr = json::array();
    for (const auto& s : ship.getAllShipments()) arr.push_back(shipmentToJson(s));
    return arr.dump();
}

std::string handleGetAffectedShipments(
    services::ShipmentService& ship,
    services::DisruptionService& dis) {
    auto disruptions = dis.getActiveDisruptions();
    json arr = json::array();
    for (const auto& s : ship.getAffectedShipments(disruptions)) arr.push_back(shipmentToJson(s));
    return arr.dump();
}

std::string handleGetDisruptions(services::DisruptionService& dis) {
    json arr = json::array();
    for (const auto& d : dis.getAllDisruptions()) arr.push_back(disruptionToJson(d));
    return arr.dump();
}

std::string handleGetRoutes(services::RouteService& route) {
    json arr = json::array();
    for (const auto& r : route.getAllRoutes()) arr.push_back(routeToJson(r));
    return arr.dump();
}

std::string handlePostRouteRecommend(
    const std::string& body,
    services::ShipmentService& ship,
    services::RouteService& route) {
    try {
        auto req = json::parse(body);
        std::string sid = req.value("shipment_id", "");
        if (sid.empty()) {
            json e; e["error"] = "shipment_id is required"; return e.dump();
        }
        auto s = ship.getById(sid);
        if (s.id.empty()) {
            json e; e["error"] = "Shipment not found: " + sid; return e.dump();
        }
        auto rec = route.recommend(sid, s.current_route, s.origin, s.destination, s.cold_chain);
        json j;
        j["shipment_id"] = sid;
        j["current_route"] = rec.current_route;
        j["recommended_route"] = rec.recommended_route;
        j["recommended_route_name"] = rec.recommended_route_name;
        j["additional_hours"] = rec.additional_hours;
        j["cost_usd"] = rec.cost_usd;
        j["risk_score"] = rec.risk_score;
        j["reason"] = rec.reason;
        j["found"] = rec.found;
        return j.dump();
    } catch (...) {
        json e; e["error"] = "Invalid request body"; return e.dump();
    }
}

std::string handleGetCarriers(services::CarrierService& carrier) {
    json arr = json::array();
    for (const auto& c : carrier.getAllCarriers()) arr.push_back(carrierToJson(c));
    return arr.dump();
}

std::string handlePostCarrierRecommend(
    const std::string& body,
    services::ShipmentService& ship,
    services::CarrierService& carrier) {
    try {
        auto req = json::parse(body);
        std::string sid = req.value("shipment_id", "");
        if (sid.empty()) {
            json e; e["error"] = "shipment_id is required"; return e.dump();
        }
        auto s = ship.getById(sid);
        if (s.id.empty()) {
            json e; e["error"] = "Shipment not found: " + sid; return e.dump();
        }
        int cap = req.value("required_capacity_kg", 5000);
        auto rec = carrier.recommend(sid, s.carrier, cap, s.cold_chain, "");
        json j;
        j["shipment_id"] = sid;
        j["current_carrier"] = rec.current_carrier;
        j["current_carrier_name"] = rec.current_carrier_name;
        j["recommended_carrier"] = rec.recommended_carrier;
        j["recommended_carrier_name"] = rec.recommended_carrier_name;
        j["reason"] = rec.reason;
        j["expected_impact"] = rec.expected_impact;
        j["found"] = rec.found;
        return j.dump();
    } catch (...) {
        json e; e["error"] = "Invalid request body"; return e.dump();
    }
}

std::string handleGetFleet(services::FleetService& fleet) {
    json arr = json::array();
    for (const auto& a : fleet.getAllAssets()) arr.push_back(assetToJson(a));
    return arr.dump();
}

std::string handleGetFleetIdle(services::FleetService& fleet) {
    json arr = json::array();
    for (const auto& a : fleet.getIdleAssets()) arr.push_back(assetToJson(a));
    return arr.dump();
}

std::string handleGetFleetUtilisation(services::FleetService& fleet) {
    auto u = fleet.getUtilisation();
    json j;
    j["total_assets"] = u.total_assets;
    j["active_assets"] = u.active_assets;
    j["idle_assets"] = u.idle_assets;
    j["in_transit_assets"] = u.in_transit_assets;
    j["maintenance_assets"] = u.maintenance_assets;
    j["average_utilisation_pct"] = static_cast<int>(u.average_utilisation_pct);
    j["underutilised_assets"] = u.underutilised_assets;
    j["redeployment_opportunities"] = u.redeployment_opportunities;
    return j.dump();
}

std::string handlePostFleetRedeploy(
    const std::string& body,
    services::FleetService& fleet) {
    try {
        auto req = json::parse(body);
        std::string asset_id = req.value("asset_id", "");
        std::string target = req.value("target_location", "");
        if (asset_id.empty()) {
            json e; e["error"] = "asset_id is required"; return e.dump();
        }
        fleet.updateStatus(asset_id, "IN_TRANSIT");
        json j;
        j["status"] = "ok";
        j["message"] = "Asset " + asset_id + " redeployment initiated to " + target;
        return j.dump();
    } catch (...) {
        json e; e["error"] = "Invalid request body"; return e.dump();
    }
}

std::string handleGetColdChainAlerts(services::ColdChainService& cold) {
    json arr = json::array();
    for (const auto& a : cold.getAllAlerts()) arr.push_back(alertToJson(a));
    return arr.dump();
}

std::string handleGetColdChainShipment(
    const std::string& shipment_id,
    services::ColdChainService& cold) {
    return alertToJson(cold.getAlert(shipment_id)).dump();
}

std::string handleGetColdChainTemperature(
    const std::string& shipment_id,
    services::ColdChainService& cold) {
    auto readings = cold.getReadings(shipment_id);
    auto cfg = cold.getConfig(shipment_id);
    json arr = json::array();
    for (const auto& r : readings) {
        json j;
        j["timestamp"] = r.timestamp;
        j["temperature"] = r.temperature;
        j["humidity"] = r.humidity;
        j["sensor_status"] = r.sensor_status;
        arr.push_back(j);
    }
    json result;
    result["readings"] = arr;
    result["config"]["min_temp"] = cfg.min_temp;
    result["config"]["max_temp"] = cfg.max_temp;
    return result.dump();
}

std::string handlePostSimulation(
    const std::string& body,
    services::SimulationService& sim) {
    try {
        auto req = json::parse(body);
        std::string scenario = req.value("scenario", "");
        std::string target = req.value("target", "");
        auto result = sim.applyScenario(scenario, target);
        json j;
        j["success"] = result.success;
        j["message"] = result.message;
        j["before"]["affected_shipments"] = result.affected_shipments_before;
        j["before"]["idle_assets"] = result.idle_assets_before;
        j["before"]["cold_chain_alerts"] = result.cold_chain_alerts_before;
        j["after"]["affected_shipments"] = result.affected_shipments_after;
        j["after"]["idle_assets"] = result.idle_assets_after;
        j["after"]["cold_chain_alerts"] = result.cold_chain_alerts_after;
        return j.dump();
    } catch (...) {
        json e; e["error"] = "Invalid request body"; return e.dump();
    }
}

std::string handleGetRecommendations(services::RecommendationService& rec_svc) {
    json arr = json::array();
    for (const auto& r : rec_svc.generateAll()) arr.push_back(recToJson(r));
    return arr.dump();
}

// ── Bob query handler ─────────────────────────────────────────────────────────
static std::string toLower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(), ::tolower);
    return r;
}

std::string handleBobQuery(
    const std::string& body,
    services::DisruptionService& dis,
    services::ShipmentService& ship,
    services::RouteService& route,
    services::CarrierService& carrier,
    services::FleetService& fleet,
    services::ColdChainService& cold,
    services::RecommendationService& rec_svc) {

    std::string question;
    try {
        auto req = json::parse(body);
        question = req.value("question", "");
    } catch (...) {
        question = body;
    }

    std::string q = toLower(question);
    std::string answer;

    auto disruptions = dis.getActiveDisruptions();
    auto affected = ship.getAffectedShipments(disruptions);

    // Dispatch based on question keywords
    if (q.find("summary") != std::string::npos ||
        q.find("what is happening") != std::string::npos ||
        q.find("situation") != std::string::npos ||
        q.find("what should we do") != std::string::npos ||
        q.find("right now") != std::string::npos ||
        q.find("action") != std::string::npos) {
        answer = rec_svc.generateBobSummary();

    } else if (q.find("affected") != std::string::npos && q.find("shipment") != std::string::npos) {
        std::ostringstream oss;
        oss << affected.size() << " shipment(s) are currently affected by active disruptions:\n\n";
        for (const auto& s : affected) {
            oss << "  " << s.id << " (" << s.origin << " → " << s.destination << ")"
                << "  Risk: " << s.risk_level << "  Score: " << s.impact_score << "\n";
        }
        answer = oss.str();

    } else if (q.find("highest risk") != std::string::npos ||
               q.find("worst") != std::string::npos ||
               q.find("most critical") != std::string::npos) {
        if (affected.empty()) {
            answer = "No shipments are currently at risk.";
        } else {
            const auto& top = affected[0];
            std::ostringstream oss;
            oss << "Highest-risk shipment: " << top.id << "\n";
            oss << "Risk: " << top.risk_level << "  (Score: " << top.impact_score << ")\n";
            oss << "Route: " << top.current_route << "  Carrier: " << top.carrier << "\n";
            oss << "Cargo: " << top.cargo_type << (top.cold_chain ? " [COLD CHAIN]" : "") << "\n";
            oss << "\nReasons:\n";
            for (const auto& r : top.impact_reasons) oss << "  - " << r << "\n";
            answer = oss.str();
        }

    } else if (q.find("route") != std::string::npos &&
               (q.find("alternative") != std::string::npos || q.find("recommend") != std::string::npos ||
                q.find("best") != std::string::npos || q.find("reroute") != std::string::npos)) {
        if (affected.empty()) {
            answer = "No affected shipments require rerouting at this time.";
        } else {
            const auto& top = affected[0];
            auto rec = route.recommend(top.id, top.current_route, top.origin, top.destination, top.cold_chain);
            std::ostringstream oss;
            oss << "Route recommendation for " << top.id << " (highest risk):\n\n";
            oss << "  Current route:     " << top.current_route << "\n";
            if (rec.found) {
                oss << "  Recommended route: " << rec.recommended_route
                    << " (" << rec.recommended_route_name << ")\n";
                oss << "  Additional time:   +" << rec.additional_hours << " hours\n";
                oss << "  Cost:              USD " << rec.cost_usd << "\n";
                oss << "  Reason:            " << rec.reason << "\n";
            } else {
                oss << "  No alternative route available: " << rec.reason << "\n";
            }
            answer = oss.str();
        }

    } else if (q.find("carrier") != std::string::npos &&
               (q.find("change") != std::string::npos || q.find("switch") != std::string::npos ||
                q.find("should we") != std::string::npos || q.find("recommend") != std::string::npos)) {
        if (affected.empty()) {
            answer = "No carrier changes are currently recommended.";
        } else {
            const auto& top = affected[0];
            auto rec = carrier.recommend(top.id, top.carrier, 5000, top.cold_chain, "");
            std::ostringstream oss;
            oss << "Carrier recommendation for " << top.id << ":\n\n";
            oss << "  Current carrier:     " << top.carrier << " (" << rec.current_carrier_name << ")\n";
            if (rec.found) {
                oss << "  Recommended carrier: " << rec.recommended_carrier
                    << " (" << rec.recommended_carrier_name << ")\n";
                oss << "  Reason: " << rec.reason << "\n";
                oss << "  Expected impact: " << rec.expected_impact << "\n";
            } else {
                oss << "  " << rec.reason << "\n";
            }
            answer = oss.str();
        }

    } else if (q.find("idle") != std::string::npos || q.find("fleet") != std::string::npos) {
        auto idle = fleet.getIdleAssets();
        std::ostringstream oss;
        oss << idle.size() << " idle/available fleet asset(s):\n\n";
        for (const auto& a : idle) {
            oss << "  " << a.id << " (" << models::FleetAsset::typeToString(a.type) << ")"
                << "  Location: " << a.location
                << "  Capacity: " << a.capacity_kg << " kg"
                << (a.refrigerated ? "  [REFRIGERATED]" : "") << "\n";
        }
        answer = oss.str();

    } else if (q.find("redeploy") != std::string::npos ||
               q.find("where should") != std::string::npos ||
               q.find("deploy") != std::string::npos) {
        auto recs = fleet.getRedeploymentRecommendations();
        std::ostringstream oss;
        if (recs.empty()) {
            oss << "No redeployment recommendations at this time.";
        } else {
            oss << "Fleet redeployment recommendations:\n\n";
            for (const auto& r : recs) {
                oss << "  Asset: " << r.asset_id << " (" << r.asset_type << ")\n";
                oss << "  From:  " << r.current_location << "\n";
                oss << "  To:    " << r.target_location << "\n";
                oss << "  Reason: " << r.reason << "\n\n";
            }
        }
        answer = oss.str();

    } else if (q.find("cold") != std::string::npos || q.find("temperature") != std::string::npos ||
               q.find("excursion") != std::string::npos || q.find("refrigerat") != std::string::npos) {
        auto alerts = cold.getAllAlerts();
        std::ostringstream oss;
        int issues = 0;
        for (const auto& a : alerts)
            if (a.severity != models::ExcursionSeverity::NORMAL) issues++;

        oss << issues << " cold-chain shipment(s) require attention:\n\n";
        for (const auto& a : alerts) {
            if (a.severity == models::ExcursionSeverity::NORMAL) continue;
            oss << "  Shipment: " << a.shipment_id << "\n";
            oss << "  Configured severity: " << models::ColdChainAlert::severityToString(a.severity) << "\n";
            oss << "  Current temp: " << a.current_temp << "°C"
                << "  Range: " << a.config.min_temp << "–" << a.config.max_temp << "°C\n";
            oss << "  " << a.severity_reason << "\n";
            oss << "  Action: " << a.recommended_action << "\n\n";
        }
        oss << "Note: Review against applicable product handling requirements.";
        answer = oss.str();

    } else if (q.find("disruption") != std::string::npos) {
        std::ostringstream oss;
        oss << disruptions.size() << " active disruption(s):\n\n";
        for (const auto& d : disruptions) {
            oss << "  " << d.name << "\n";
            oss << "  Type: " << models::Disruption::typeToString(d.type) << "\n";
            oss << "  Location: " << d.location << "\n";
            oss << "  Severity: " << models::Disruption::severityToString(d.severity) << "\n";
            oss << "  " << d.description << "\n\n";
        }
        answer = oss.str();

    } else {
        // Default: full summary
        answer = rec_svc.generateBobSummary();
    }

    json j;
    j["question"] = question;
    j["answer"] = answer;
    return j.dump();
}

} // namespace api
