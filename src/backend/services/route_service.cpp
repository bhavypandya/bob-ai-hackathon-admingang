#include "route_service.h"
#include "shipment_service.h"
#include <algorithm>
#include <cmath>

namespace services {

RouteService::RouteService(db::Database& db) : db_(db) {}

models::Route RouteService::rowToRoute(int /*col*/, const char** vals, const char** /*names*/) {
    models::Route r;
    r.id = vals[0] ? vals[0] : "";
    r.name = vals[1] ? vals[1] : "";
    r.origin = vals[2] ? vals[2] : "";
    r.destination = vals[3] ? vals[3] : "";
    r.estimated_hours = vals[4] ? std::stod(vals[4]) : 0.0;
    r.total_distance_km = vals[5] ? std::stod(vals[5]) : 0.0;
    r.cost_usd = vals[6] ? std::stod(vals[6]) : 0.0;
    r.capacity_kg = vals[7] ? std::stoi(vals[7]) : 0;
    r.is_disrupted = vals[8] && std::string(vals[8]) == "1";
    r.disruption_reason = vals[9] ? vals[9] : "";
    r.risk_score = vals[10] ? std::stoi(vals[10]) : 0;
    r.refrigerated_capable = vals[11] && std::string(vals[11]) == "1";
    return r;
}

std::vector<models::Route> RouteService::getAllRoutes() {
    std::vector<models::Route> result;
    db_.query("SELECT * FROM routes ORDER BY id",
        [&](int c, const char** v, const char** n) {
            result.push_back(rowToRoute(c, v, n));
        });
    return result;
}

models::Route RouteService::getById(const std::string& id) {
    models::Route r;
    r.id = "";
    db_.query("SELECT * FROM routes WHERE id='" + id + "'",
        [&](int c, const char** v, const char** n) {
            r = rowToRoute(c, v, n);
        });
    return r;
}

RouteRecommendation RouteService::recommend(
    const std::string& shipment_id,
    const std::string& current_route_id,
    const std::string& origin,
    const std::string& destination,
    bool needs_refrigeration) {

    RouteRecommendation rec;
    rec.shipment_id = shipment_id;
    rec.current_route = current_route_id;
    rec.found = false;
    rec.additional_hours = 0;

    // Get current route info
    models::Route current = getById(current_route_id);
    if (current.id.empty()) {
        rec.reason = "Current route not found in database.";
        return rec;
    }

    // Get all routes that serve the same or equivalent corridor
    // Strategy: find routes from same origin/destination that are NOT disrupted
    std::string sql = "SELECT * FROM routes WHERE is_disrupted=0 AND id != '" + current_route_id + "'";
    if (needs_refrigeration) sql += " AND refrigerated=1";
    sql += " ORDER BY risk_score ASC, estimated_hours ASC";

    std::vector<models::Route> candidates;
    db_.query(sql, [&](int c, const char** v, const char** n) {
        models::Route r = rowToRoute(c, v, n);
        // Accept route if origin/destination match (or overlaps with a hub)
        bool origin_match = (r.origin == origin || r.origin == current.origin);
        bool dest_match = (r.destination == destination || r.destination == current.destination);
        if (origin_match && dest_match) {
            candidates.push_back(r);
        }
    });

    // If no exact match, find routes that go close
    if (candidates.empty()) {
        db_.query("SELECT * FROM routes WHERE is_disrupted=0 AND id != '" + current_route_id + "' ORDER BY risk_score ASC, cost_usd ASC",
            [&](int c, const char** v, const char** n) {
                models::Route r = rowToRoute(c, v, n);
                // Use geography heuristic: any route from origin OR to destination
                if ((r.origin == origin || r.destination == destination) && candidates.size() < 3) {
                    candidates.push_back(r);
                }
            });
    }

    if (candidates.empty()) {
        rec.reason = "No available alternative route found for this origin–destination pair.";
        return rec;
    }

    // Pick best candidate: lowest risk_score, then least extra time
    models::Route best = candidates[0];
    for (const auto& c : candidates) {
        int c_score = c.risk_score * 100 + static_cast<int>(c.estimated_hours);
        int b_score = best.risk_score * 100 + static_cast<int>(best.estimated_hours);
        if (c_score < b_score) best = c;
    }

    rec.recommended_route = best.id;
    rec.recommended_route_name = best.name;
    rec.cost_usd = best.cost_usd;
    rec.risk_score = best.risk_score;
    rec.additional_hours = static_cast<int>(std::max(0.0, best.estimated_hours - current.estimated_hours));
    rec.found = true;
    rec.reason = best.name + " avoids the active disruption";
    if (best.risk_score < current.risk_score) rec.reason += " and has lower risk";
    rec.reason += ". Available capacity: " + std::to_string(best.capacity_kg) + " kg.";

    return rec;
}

} // namespace services
