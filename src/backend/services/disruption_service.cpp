#include "disruption_service.h"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace services {

DisruptionService::DisruptionService(db::Database& db) : db_(db) {}

models::Disruption DisruptionService::rowToDisruption(int /*col*/, const char** vals, const char** /*names*/) {
    models::Disruption d;
    d.id = vals[0] ? std::stoi(vals[0]) : 0;
    std::string type = vals[1] ? vals[1] : "";
    if (type == "Weather Event") d.type = models::DisruptionType::WEATHER_EVENT;
    else if (type == "Port Strike") d.type = models::DisruptionType::PORT_STRIKE;
    else if (type == "Geopolitical Crisis") d.type = models::DisruptionType::GEOPOLITICAL_CRISIS;
    else if (type == "Road Closure") d.type = models::DisruptionType::ROAD_CLOSURE;
    else d.type = models::DisruptionType::CARRIER_DISRUPTION;

    d.name = vals[2] ? vals[2] : "";
    d.location = vals[3] ? vals[3] : "";

    std::string sev = vals[4] ? vals[4] : "";
    if (sev == "CRITICAL") d.severity = models::DisruptionSeverity::CRITICAL;
    else if (sev == "HIGH") d.severity = models::DisruptionSeverity::HIGH;
    else if (sev == "MEDIUM") d.severity = models::DisruptionSeverity::MEDIUM;
    else d.severity = models::DisruptionSeverity::LOW;

    d.start_time = vals[5] ? vals[5] : "";
    d.expected_end_time = vals[6] ? vals[6] : "";

    // Parse comma-separated affected routes
    std::string routes_str = vals[7] ? vals[7] : "";
    std::stringstream ss(routes_str);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) d.affected_routes.push_back(tok);
    }

    d.description = vals[8] ? vals[8] : "";

    std::string status = vals[9] ? vals[9] : "ACTIVE";
    if (status == "MONITORING") d.status = models::DisruptionStatus::MONITORING;
    else if (status == "RESOLVED") d.status = models::DisruptionStatus::RESOLVED;
    else d.status = models::DisruptionStatus::ACTIVE;

    return d;
}

std::vector<models::Disruption> DisruptionService::getAllDisruptions() {
    std::vector<models::Disruption> result;
    db_.query("SELECT * FROM disruptions ORDER BY id",
        [&](int c, const char** v, const char** n) {
            result.push_back(rowToDisruption(c, v, n));
        });
    return result;
}

std::vector<models::Disruption> DisruptionService::getActiveDisruptions() {
    std::vector<models::Disruption> result;
    db_.query("SELECT * FROM disruptions WHERE status IN ('ACTIVE','MONITORING') ORDER BY id",
        [&](int c, const char** v, const char** n) {
            result.push_back(rowToDisruption(c, v, n));
        });
    return result;
}

models::Disruption DisruptionService::getById(int id) {
    models::Disruption result{};
    result.id = -1;
    db_.query("SELECT * FROM disruptions WHERE id = " + std::to_string(id),
        [&](int c, const char** v, const char** n) {
            result = rowToDisruption(c, v, n);
        });
    return result;
}

void DisruptionService::setStatus(int disruption_id, const std::string& status) {
    db_.exec("UPDATE disruptions SET status='" + status + "' WHERE id=" + std::to_string(disruption_id));
}

int DisruptionService::addDisruption(const models::Disruption& d) {
    std::string routes;
    for (size_t i = 0; i < d.affected_routes.size(); ++i) {
        if (i > 0) routes += ",";
        routes += d.affected_routes[i];
    }
    std::string sql = "INSERT INTO disruptions (type,name,location,severity,start_time,end_time,affected_routes,description,status) VALUES ('"
        + models::Disruption::typeToString(d.type) + "','"
        + d.name + "','"
        + d.location + "','"
        + models::Disruption::severityToString(d.severity) + "','"
        + d.start_time + "','"
        + d.expected_end_time + "','"
        + routes + "','"
        + d.description + "','"
        + models::Disruption::statusToString(d.status) + "')";
    db_.exec(sql);
    int new_id = 0;
    db_.query("SELECT last_insert_rowid()", [&](int, const char** v, const char**) {
        new_id = v[0] ? std::stoi(v[0]) : 0;
    });
    return new_id;
}

void DisruptionService::setRouteDisrupted(const std::string& route_id, bool disrupted, const std::string& reason) {
    std::string sql = "UPDATE routes SET is_disrupted=" + std::to_string(disrupted ? 1 : 0)
        + ", disruption_reason='" + reason + "'"
        + " WHERE id='" + route_id + "'";
    db_.exec(sql);
}

void DisruptionService::setCarrierDisrupted(const std::string& carrier_id, bool disrupted, const std::string& reason) {
    std::string sql = "UPDATE carriers SET is_disrupted=" + std::to_string(disrupted ? 1 : 0)
        + ", disruption_reason='" + reason + "'"
        + " WHERE id='" + carrier_id + "'";
    db_.exec(sql);
}

std::vector<std::string> DisruptionService::getAffectedShipmentIds(const models::Disruption& d) {
    std::vector<std::string> ids;
    if (d.affected_routes.empty()) return ids;

    // Build IN clause
    std::string in_clause = "(";
    for (size_t i = 0; i < d.affected_routes.size(); ++i) {
        if (i > 0) in_clause += ",";
        in_clause += "'" + d.affected_routes[i] + "'";
    }
    in_clause += ")";

    db_.query("SELECT id FROM shipments WHERE current_route IN " + in_clause + " AND status != 'DELIVERED'",
        [&](int, const char** v, const char**) {
            if (v[0]) ids.push_back(v[0]);
        });
    return ids;
}

} // namespace services
