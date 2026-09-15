#include "carrier_service.h"
#include <algorithm>

namespace services {

CarrierService::CarrierService(db::Database& db) : db_(db) {}

models::Carrier CarrierService::rowToCarrier(int /*col*/, const char** vals, const char** /*names*/) {
    models::Carrier c;
    c.id = vals[0] ? vals[0] : "";
    c.name = vals[1] ? vals[1] : "";
    c.type = vals[2] ? vals[2] : "";
    c.available_capacity_kg = vals[3] ? std::stoi(vals[3]) : 0;
    c.total_capacity_kg = vals[4] ? std::stoi(vals[4]) : 0;
    c.reliability_score = vals[5] ? std::stod(vals[5]) : 0.0;
    c.cost_per_km = vals[6] ? std::stod(vals[6]) : 0.0;
    c.avg_delay_hours = vals[7] ? std::stoi(vals[7]) : 0;
    c.cold_chain_capable = vals[8] && std::string(vals[8]) == "1";
    c.is_disrupted = vals[9] && std::string(vals[9]) == "1";
    c.disruption_reason = vals[10] ? vals[10] : "";
    c.service_region = vals[11] ? vals[11] : "";
    return c;
}

std::vector<models::Carrier> CarrierService::getAllCarriers() {
    std::vector<models::Carrier> result;
    db_.query("SELECT * FROM carriers ORDER BY id",
        [&](int c, const char** v, const char** n) {
            result.push_back(rowToCarrier(c, v, n));
        });
    return result;
}

models::Carrier CarrierService::getById(const std::string& id) {
    models::Carrier c;
    c.id = "";
    db_.query("SELECT * FROM carriers WHERE id='" + id + "'",
        [&](int col, const char** v, const char** n) {
            c = rowToCarrier(col, v, n);
        });
    return c;
}

CarrierRecommendation CarrierService::recommend(
    const std::string& shipment_id,
    const std::string& current_carrier_id,
    int required_capacity_kg,
    bool needs_cold_chain,
    const std::string& /*service_region*/) {

    CarrierRecommendation rec;
    rec.shipment_id = shipment_id;
    rec.current_carrier = current_carrier_id;
    rec.found = false;

    models::Carrier current = getById(current_carrier_id);
    if (!current.id.empty()) {
        rec.current_carrier_name = current.name;
    }

    // If current carrier is OK, no need to switch
    if (!current.id.empty() && !current.is_disrupted &&
        current.available_capacity_kg >= required_capacity_kg) {
        rec.reason = "Current carrier " + current.name + " is available and has sufficient capacity.";
        rec.recommended_carrier = current_carrier_id;
        rec.recommended_carrier_name = current.name;
        rec.found = true;
        return rec;
    }

    // Build query for alternative carriers
    std::string sql = "SELECT * FROM carriers WHERE is_disrupted=0 AND id != '" + current_carrier_id
        + "' AND available_capacity >= " + std::to_string(required_capacity_kg);
    if (needs_cold_chain) sql += " AND cold_chain=1";
    sql += " ORDER BY reliability DESC, cost_per_km ASC";

    std::vector<models::Carrier> candidates;
    db_.query(sql, [&](int col, const char** v, const char** n) {
        candidates.push_back(rowToCarrier(col, v, n));
    });

    if (candidates.empty()) {
        // Relax capacity requirement
        std::string sql2 = "SELECT * FROM carriers WHERE is_disrupted=0 AND id != '" + current_carrier_id + "'";
        if (needs_cold_chain) sql2 += " AND cold_chain=1";
        sql2 += " ORDER BY available_capacity DESC, reliability DESC LIMIT 3";
        db_.query(sql2, [&](int col, const char** v, const char** n) {
            candidates.push_back(rowToCarrier(col, v, n));
        });
    }

    if (candidates.empty()) {
        rec.reason = "No suitable alternative carrier available with the required capabilities.";
        return rec;
    }

    // Score: reliability * 50 - avg_delay * 5 - cost_per_km * 2
    models::Carrier best = candidates[0];
    double best_score = best.reliability_score * 50.0 - best.avg_delay_hours * 5.0 - best.cost_per_km * 0.5;
    for (const auto& c : candidates) {
        double score = c.reliability_score * 50.0 - c.avg_delay_hours * 5.0 - c.cost_per_km * 0.5;
        if (score > best_score) {
            best_score = score;
            best = c;
        }
    }

    rec.recommended_carrier = best.id;
    rec.recommended_carrier_name = best.name;
    rec.found = true;

    std::string impact_reason = "";
    if (current.is_disrupted) {
        impact_reason = "Current carrier " + current.name + " is disrupted. ";
    } else {
        impact_reason = "Current carrier has insufficient capacity. ";
    }

    rec.reason = impact_reason + best.name + " has reliability " +
        std::to_string(static_cast<int>(best.reliability_score * 100)) + "% and " +
        std::to_string(best.available_capacity_kg) + " kg available capacity.";

    if (best.avg_delay_hours < current.avg_delay_hours) {
        rec.expected_impact = "Expected average delay improvement: " +
            std::to_string(current.avg_delay_hours - best.avg_delay_hours) + " hours.";
    } else {
        rec.expected_impact = "Carrier switch maintains service continuity.";
    }

    return rec;
}

} // namespace services
