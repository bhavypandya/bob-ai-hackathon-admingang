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

// ── Driver / Request / Timeline handlers ──────────────────────────────────────

static json driverRequestToJson(int nc, const char** vals, const char** cols) {
    json j;
    for (int i = 0; i < nc; ++i) {
        std::string col(cols[i]);
        std::string val = vals[i] ? vals[i] : "";
        j[col] = val;
    }
    return j;
}

std::string handleGetDrivers(db::Database& db) {
    json arr = json::array();
    db.query("SELECT id,name,contact,vehicle_id,status FROM drivers ORDER BY id",
        [&arr](int nc, const char** vals, const char** cols) {
            json j;
            for (int i = 0; i < nc; ++i) j[cols[i]] = vals[i] ? vals[i] : "";
            arr.push_back(j);
        });
    return arr.dump();
}

std::string handleGetDriver(const std::string& driver_id, db::Database& db) {
    json result;
    result["id"] = "";
    db.query("SELECT id,name,contact,vehicle_id,status FROM drivers WHERE id='" + driver_id + "'",
        [&result](int nc, const char** vals, const char** cols) {
            for (int i = 0; i < nc; ++i) result[cols[i]] = vals[i] ? vals[i] : "";
        });
    if (result["id"].get<std::string>().empty()) {
        json e; e["error"] = "Driver not found"; return e.dump();
    }
    return result.dump();
}

std::string handleGetDriverShipments(const std::string& driver_id, db::Database& db,
                                      services::ShipmentService& ship) {
    // Get vehicle assigned to driver
    std::string vehicle_id;
    db.query("SELECT vehicle_id FROM drivers WHERE id='" + driver_id + "'",
        [&vehicle_id](int, const char** vals, const char**) {
            if (vals[0]) vehicle_id = vals[0];
        });

    json arr = json::array();
    if (!vehicle_id.empty()) {
        for (const auto& s : ship.getAllShipments()) {
            if (s.fleet_asset == vehicle_id) {
                json j;
                j["id"] = s.id;
                j["origin"] = s.origin;
                j["destination"] = s.destination;
                j["status"] = models::Shipment::statusToString(s.status);
                j["priority"] = models::Shipment::priorityToString(s.priority);
                j["cargo_type"] = s.cargo_type;
                j["cold_chain"] = s.cold_chain;
                j["planned_departure"] = s.planned_departure;
                j["planned_arrival"] = s.planned_arrival;
                j["current_eta"] = s.current_eta;
                j["expected_delay_hours"] = s.expected_delay_hours;
                j["carrier"] = s.carrier;
                j["fleet_asset"] = s.fleet_asset;
                j["current_route"] = s.current_route;
                arr.push_back(j);
            }
        }
    }
    return arr.dump();
}

std::string handleGetDriverRequests(db::Database& db) {
    json arr = json::array();
    db.query(R"(SELECT dr.id, dr.driver_id, d.name as driver_name, dr.shipment_id,
                       dr.request_type, dr.message, dr.location, dr.created_at,
                       dr.status, dr.admin_response, dr.updated_at
                FROM driver_requests dr
                LEFT JOIN drivers d ON dr.driver_id = d.id
                ORDER BY dr.created_at DESC)",
        [&arr](int nc, const char** vals, const char** cols) {
            json j;
            for (int i = 0; i < nc; ++i) j[cols[i]] = vals[i] ? vals[i] : "";
            arr.push_back(j);
        });
    return arr.dump();
}

std::string handlePostDriverRequest(const std::string& body, db::Database& db) {
    try {
        auto req = json::parse(body);
        std::string driver_id  = req.value("driver_id", "");
        std::string shipment_id = req.value("shipment_id", "");
        std::string request_type = req.value("request_type", "Other");
        std::string message    = req.value("message", "");
        std::string location   = req.value("location", "");

        if (driver_id.empty() || shipment_id.empty()) {
            json e; e["error"] = "driver_id and shipment_id are required"; return e.dump();
        }

        // Generate a new request ID
        int count = 0;
        db.query("SELECT COUNT(*) FROM driver_requests",
            [&count](int, const char** vals, const char**) {
                if (vals[0]) count = std::stoi(vals[0]);
            });
        std::string req_id = "REQ" + std::to_string(1000 + count + 1);

        // Get current timestamp placeholder
        std::string now = "2024-06-10 " + std::to_string(18 + (count % 6)) + ":00";

        // Escape quotes
        auto esc = [](std::string s) {
            std::string r;
            for (char c : s) { if (c == '\'') r += "''"; else r += c; }
            return r;
        };

        db.exec("INSERT INTO driver_requests VALUES ('" + req_id + "','" + esc(driver_id) +
                "','" + esc(shipment_id) + "','" + esc(request_type) + "','" + esc(message) +
                "','" + esc(location) + "','" + now + "','PENDING','','')");

        // Create a pending notification
        std::string notif_id = "NOTIF_" + req_id;
        db.exec("INSERT INTO notifications VALUES ('" + notif_id + "','" + esc(driver_id) +
                "','Request Submitted','Your " + esc(request_type) + " request " + req_id +
                " has been submitted and is awaiting admin review.','REQUEST_PENDING','" +
                req_id + "','" + esc(shipment_id) + "','" + now + "',0)");

        json j;
        j["status"] = "ok";
        j["request_id"] = req_id;
        j["message"] = "Request submitted. Awaiting admin approval.";
        return j.dump();
    } catch (...) {
        json e; e["error"] = "Invalid request body"; return e.dump();
    }
}

std::string handleGetDriverRequest(const std::string& request_id, db::Database& db) {
    json result;
    result["id"] = "";
    db.query(R"(SELECT dr.id, dr.driver_id, d.name as driver_name, dr.shipment_id,
                       dr.request_type, dr.message, dr.location, dr.created_at,
                       dr.status, dr.admin_response, dr.updated_at
                FROM driver_requests dr
                LEFT JOIN drivers d ON dr.driver_id = d.id
                WHERE dr.id=')" + request_id + "'",
        [&result](int nc, const char** vals, const char** cols) {
            for (int i = 0; i < nc; ++i) result[cols[i]] = vals[i] ? vals[i] : "";
        });
    if (result["id"].get<std::string>().empty()) {
        json e; e["error"] = "Request not found"; return e.dump();
    }
    return result.dump();
}

std::string handleApproveDriverRequest(const std::string& request_id,
                                        const std::string& body, db::Database& db) {
    std::string admin_response;
    try {
        auto req = json::parse(body);
        admin_response = req.value("admin_response", "Request approved.");
    } catch (...) {
        admin_response = "Request approved.";
    }

    auto esc = [](std::string s) {
        std::string r;
        for (char c : s) { if (c == '\'') r += "''"; else r += c; }
        return r;
    };

    db.exec("UPDATE driver_requests SET status='APPROVED', admin_response='" +
            esc(admin_response) + "', updated_at='2024-06-10 18:30' WHERE id='" + request_id + "'");

    // Get driver_id and shipment_id for notification
    std::string driver_id, shipment_id, req_type;
    db.query("SELECT driver_id, shipment_id, request_type FROM driver_requests WHERE id='" +
             request_id + "'",
        [&](int, const char** vals, const char**) {
            if (vals[0]) driver_id = vals[0];
            if (vals[1]) shipment_id = vals[1];
            if (vals[2]) req_type = vals[2];
        });

    if (!driver_id.empty()) {
        // Mark old pending notification read
        db.exec("UPDATE notifications SET read_status=1 WHERE related_request_id='" +
                request_id + "' AND type='REQUEST_PENDING'");

        std::string notif_id = "NOTIFAPP_" + request_id;
        db.exec("INSERT OR REPLACE INTO notifications VALUES ('" + notif_id + "','" +
                esc(driver_id) + "','Request Approved','" +
                "Your " + esc(req_type) + " request " + request_id +
                " has been approved by the operations team. " + esc(admin_response) +
                "','REQUEST_APPROVED','" + request_id + "','" + esc(shipment_id) +
                "','2024-06-10 18:30',0)");
    }

    json j;
    j["status"] = "ok";
    j["message"] = "Request " + request_id + " approved.";
    return j.dump();
}

std::string handleRejectDriverRequest(const std::string& request_id,
                                       const std::string& body, db::Database& db) {
    std::string admin_response;
    try {
        auto req = json::parse(body);
        admin_response = req.value("admin_response", "Request rejected.");
    } catch (...) {
        admin_response = "Request rejected.";
    }

    auto esc = [](std::string s) {
        std::string r;
        for (char c : s) { if (c == '\'') r += "''"; else r += c; }
        return r;
    };

    db.exec("UPDATE driver_requests SET status='REJECTED', admin_response='" +
            esc(admin_response) + "', updated_at='2024-06-10 18:35' WHERE id='" + request_id + "'");

    std::string driver_id, shipment_id, req_type;
    db.query("SELECT driver_id, shipment_id, request_type FROM driver_requests WHERE id='" +
             request_id + "'",
        [&](int, const char** vals, const char**) {
            if (vals[0]) driver_id = vals[0];
            if (vals[1]) shipment_id = vals[1];
            if (vals[2]) req_type = vals[2];
        });

    if (!driver_id.empty()) {
        db.exec("UPDATE notifications SET read_status=1 WHERE related_request_id='" +
                request_id + "' AND type='REQUEST_PENDING'");

        std::string notif_id = "NOTIFREJ_" + request_id;
        db.exec("INSERT OR REPLACE INTO notifications VALUES ('" + notif_id + "','" +
                esc(driver_id) + "','Request Rejected','" +
                "Your " + esc(req_type) + " request " + request_id +
                " was rejected. Reason: " + esc(admin_response) +
                "','REQUEST_REJECTED','" + request_id + "','" + esc(shipment_id) +
                "','2024-06-10 18:35',0)");
    }

    json j;
    j["status"] = "ok";
    j["message"] = "Request " + request_id + " rejected.";
    return j.dump();
}

std::string handleGetNotifications(const std::string& driver_id, db::Database& db) {
    json arr = json::array();
    db.query("SELECT id,driver_id,title,message,type,related_request_id,related_shipment_id,"
             "created_at,read_status FROM notifications WHERE driver_id='" + driver_id +
             "' ORDER BY created_at DESC",
        [&arr](int nc, const char** vals, const char** cols) {
            json j;
            for (int i = 0; i < nc; ++i) {
                if (std::string(cols[i]) == "read_status")
                    j[cols[i]] = vals[i] ? (std::string(vals[i]) == "1") : false;
                else
                    j[cols[i]] = vals[i] ? vals[i] : "";
            }
            arr.push_back(j);
        });
    return arr.dump();
}

std::string handleMarkNotificationRead(const std::string& notif_id, db::Database& db) {
    db.exec("UPDATE notifications SET read_status=1 WHERE id='" + notif_id + "'");
    json j; j["status"] = "ok"; return j.dump();
}

std::string handleGetShipmentTimeline(const std::string& shipment_id, db::Database& db) {
    json result;
    result["shipment_id"] = shipment_id;

    // Get shipment base info
    db.query("SELECT id,origin,destination,status,planned_departure,planned_arrival,"
             "current_eta,expected_delay_hours,cargo_type,carrier,current_route FROM shipments WHERE id='" +
             shipment_id + "'",
        [&result](int nc, const char** vals, const char** cols) {
            for (int i = 0; i < nc; ++i) result[cols[i]] = vals[i] ? vals[i] : "";
        });

    // Checkpoints
    json checkpoints = json::array();
    db.query("SELECT id,shipment_id,name,latitude,longitude,sequence,expected_arrival,"
             "actual_arrival,departure_time,status FROM shipment_checkpoints "
             "WHERE shipment_id='" + shipment_id + "' ORDER BY sequence",
        [&checkpoints](int nc, const char** vals, const char** cols) {
            json j;
            for (int i = 0; i < nc; ++i) j[cols[i]] = vals[i] ? vals[i] : "";
            checkpoints.push_back(j);
        });
    result["checkpoints"] = checkpoints;

    // Current location
    json location;
    location["available"] = false;
    db.query("SELECT location_name,latitude,longitude,timestamp,source,accuracy_status "
             "FROM shipment_locations WHERE shipment_id='" + shipment_id +
             "' ORDER BY timestamp DESC LIMIT 1",
        [&location](int nc, const char** vals, const char** cols) {
            location["available"] = true;
            for (int i = 0; i < nc; ++i) location[cols[i]] = vals[i] ? vals[i] : "";
        });
    result["current_location"] = location;

    // Disruption events
    json disruptions = json::array();
    db.query("SELECT id,location,latitude,longitude,detected_at,type,severity,description "
             "FROM disruption_events WHERE shipment_id='" + shipment_id + "'",
        [&disruptions](int nc, const char** vals, const char** cols) {
            json j;
            for (int i = 0; i < nc; ++i) j[cols[i]] = vals[i] ? vals[i] : "";
            disruptions.push_back(j);
        });
    result["disruption_events"] = disruptions;

    // Reroutes
    json reroutes = json::array();
    db.query("SELECT id,from_location,disruption_location,original_route,alternate_route,"
             "created_at,new_eta,status FROM reroutes WHERE shipment_id='" + shipment_id + "'",
        [&reroutes](int nc, const char** vals, const char** cols) {
            json j;
            for (int i = 0; i < nc; ++i) j[cols[i]] = vals[i] ? vals[i] : "";
            reroutes.push_back(j);
        });
    result["reroutes"] = reroutes;

    // Driver requests related to this shipment
    json requests = json::array();
    db.query(R"(SELECT dr.id,dr.driver_id,d.name as driver_name,dr.request_type,
                       dr.message,dr.location,dr.created_at,dr.status,dr.admin_response
                FROM driver_requests dr LEFT JOIN drivers d ON dr.driver_id=d.id
                WHERE dr.shipment_id=')" + shipment_id + "' ORDER BY dr.created_at",
        [&requests](int nc, const char** vals, const char** cols) {
            json j;
            for (int i = 0; i < nc; ++i) j[cols[i]] = vals[i] ? vals[i] : "";
            requests.push_back(j);
        });
    result["driver_requests"] = requests;

    return result.dump();
}

std::string handleGetShipmentLocation(const std::string& shipment_id, db::Database& db) {
    json result;
    result["shipment_id"] = shipment_id;
    result["available"] = false;
    db.query("SELECT location_name,latitude,longitude,timestamp,source,accuracy_status "
             "FROM shipment_locations WHERE shipment_id='" + shipment_id +
             "' ORDER BY timestamp DESC LIMIT 1",
        [&result](int nc, const char** vals, const char** cols) {
            result["available"] = true;
            for (int i = 0; i < nc; ++i) result[cols[i]] = vals[i] ? vals[i] : "";
        });
    return result.dump();
}

std::string handleGetShipmentRoute(const std::string& shipment_id, db::Database& db,
                                    services::ShipmentService& ship) {
    json result;
    result["shipment_id"] = shipment_id;

    auto s = ship.getById(shipment_id);
    result["origin"] = s.origin;
    result["destination"] = s.destination;
    result["current_route"] = s.current_route;
    result["status"] = models::Shipment::statusToString(s.status);

    json checkpoints = json::array();
    db.query("SELECT name,sequence,status,expected_arrival,actual_arrival FROM shipment_checkpoints "
             "WHERE shipment_id='" + shipment_id + "' ORDER BY sequence",
        [&checkpoints](int nc, const char** vals, const char** cols) {
            json j;
            for (int i = 0; i < nc; ++i) j[cols[i]] = vals[i] ? vals[i] : "";
            checkpoints.push_back(j);
        });
    result["checkpoints"] = checkpoints;

    json reroutes = json::array();
    db.query("SELECT alternate_route,new_eta,status FROM reroutes WHERE shipment_id='" +
             shipment_id + "'",
        [&reroutes](int nc, const char** vals, const char** cols) {
            json j;
            for (int i = 0; i < nc; ++i) j[cols[i]] = vals[i] ? vals[i] : "";
            reroutes.push_back(j);
        });
    result["reroutes"] = reroutes;

    return result.dump();
}

std::string handleGetDisruptionVerification(const std::string& disruption_event_id,
                                             db::Database& db) {
    json result;
    result["disruption_event_id"] = disruption_event_id;

    // Get disruption event info
    db.query("SELECT id,shipment_id,location,detected_at,type,severity,description "
             "FROM disruption_events WHERE id='" + disruption_event_id + "'",
        [&result](int nc, const char** vals, const char** cols) {
            for (int i = 0; i < nc; ++i) result[cols[i]] = vals[i] ? vals[i] : "";
        });

    // Get verification sources
    json sources = json::array();
    db.query("SELECT id,source_name,source_type,source_url,published_at,summary,status "
             "FROM verification_sources WHERE disruption_event_id='" + disruption_event_id + "'",
        [&sources](int nc, const char** vals, const char** cols) {
            json j;
            for (int i = 0; i < nc; ++i) j[cols[i]] = vals[i] ? vals[i] : "";
            sources.push_back(j);
        });
    result["sources"] = sources;

    // Calculate verification status
    int supported = 0;
    for (const auto& s : sources) {
        if (s.value("status","") == "SUPPORTED") supported++;
    }

    std::string verification_status;
    if (sources.empty()) {
        verification_status = "UNVERIFIED";
    } else if (supported == 0) {
        verification_status = "UNVERIFIED";
    } else if (supported == 1) {
        verification_status = "SINGLE_SOURCE";
    } else {
        verification_status = "MULTI_SOURCE";
    }
    result["verification_status"] = verification_status;
    result["supported_sources"] = supported;
    result["total_sources"] = (int)sources.size();

    std::string label;
    if (verification_status == "MULTI_SOURCE") label = "SUPPORTED BY MULTIPLE SOURCES";
    else if (verification_status == "SINGLE_SOURCE") label = "SINGLE-SOURCE SUPPORT";
    else label = "UNVERIFIED — No independent supporting source found";
    result["verification_label"] = label;

    return result.dump();
}

std::string handlePostShipmentReroute(const std::string& shipment_id,
                                       const std::string& body, db::Database& db,
                                       services::ShipmentService& ship) {
    try {
        auto req = json::parse(body);
        std::string from_location = req.value("from_location", "Current Location");
        std::string disruption_location = req.value("disruption_location", "");
        std::string original_route = req.value("original_route", "");
        std::string alternate_route = req.value("alternate_route", "");
        std::string new_eta = req.value("new_eta", "");

        auto esc = [](std::string s) {
            std::string r;
            for (char c : s) { if (c == '\'') r += "''"; else r += c; }
            return r;
        };

        int count = 0;
        db.query("SELECT COUNT(*) FROM reroutes",
            [&count](int, const char** vals, const char**) {
                if (vals[0]) count = std::stoi(vals[0]);
            });
        std::string rr_id = "RR" + std::to_string(count + 1).insert(0, 3 - std::to_string(count+1).size(), '0');

        db.exec("INSERT INTO reroutes VALUES ('" + rr_id + "','" + esc(shipment_id) +
                "','" + esc(from_location) + "','" + esc(disruption_location) +
                "','" + esc(original_route) + "','" + esc(alternate_route) +
                "','2024-06-10 18:25','" + esc(new_eta) + "','ACTIVE')");

        if (!new_eta.empty()) {
            db.exec("UPDATE shipments SET current_eta='" + esc(new_eta) + "' WHERE id='" +
                    esc(shipment_id) + "'");
        }

        json j;
        j["status"] = "ok";
        j["reroute_id"] = rr_id;
        j["message"] = "Reroute created for shipment " + shipment_id;
        return j.dump();
    } catch (...) {
        json e; e["error"] = "Invalid request body"; return e.dump();
    }
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
    services::RecommendationService& rec_svc,
    db::Database& db) {

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

    // ── New queries: shipment timeline, driver requests, location ────────────
    } else if ((q.find("sh1042") != std::string::npos || q.find("sh 1042") != std::string::npos) ||
               (q.find("timeline") != std::string::npos && q.find("shipment") != std::string::npos)) {
        // Which shipment are they asking about?
        std::string sid = "SH1042";
        // Try to extract explicit SH-ID
        for (const auto& s : ship.getAllShipments()) {
            if (q.find(toLower(s.id)) != std::string::npos) { sid = s.id; break; }
        }
        std::ostringstream oss;
        oss << "Shipment " << sid << " Timeline\n";
        oss << "─────────────────────────────────\n\n";

        // Base info
        auto s = ship.getById(sid);
        if (!s.id.empty()) {
            oss << "Origin:       " << s.origin << "\n";
            oss << "Destination:  " << s.destination << "\n";
            oss << "Status:       " << models::Shipment::statusToString(s.status) << "\n";
            oss << "Departure:    " << s.planned_departure << "\n";
            oss << "Original ETA: " << s.planned_arrival << "\n";
            oss << "Current ETA:  " << s.current_eta << "\n";
            if (s.expected_delay_hours > 0)
                oss << "Delay:        +" << s.expected_delay_hours << " hours\n";
            oss << "\n";
        }

        // Checkpoints
        bool has_checkpoints = false;
        db.query("SELECT name,sequence,expected_arrival,actual_arrival,departure_time,status "
                 "FROM shipment_checkpoints WHERE shipment_id='" + sid + "' ORDER BY sequence",
            [&oss, &has_checkpoints](int nc, const char** vals, const char** cols) {
                has_checkpoints = true;
                std::string name = vals[0] ? vals[0] : "";
                std::string exp  = vals[2] ? vals[2] : "";
                std::string act  = vals[3] ? vals[3] : "";
                std::string dep  = vals[4] ? vals[4] : "";
                std::string st   = vals[5] ? vals[5] : "";
                std::string icon = "○";
                if (st == "COMPLETED") icon = "✓";
                else if (st == "CURRENT") icon = "📍";
                else if (st == "DISRUPTED") icon = "⚠";
                else if (st == "REROUTED") icon = "↪";
                oss << icon << " " << name << "  [" << st << "]\n";
                if (!act.empty())  oss << "   Arrived:   " << act << "\n";
                else if (!exp.empty()) oss << "   Expected:  " << exp << "\n";
                if (!dep.empty())  oss << "   Departed:  " << dep << "\n";
                oss << "\n";
            });

        if (!has_checkpoints) {
            oss << "No checkpoint data available for this shipment.\n\n";
        }

        // Location
        std::string loc_name;
        db.query("SELECT location_name,timestamp,accuracy_status FROM shipment_locations "
                 "WHERE shipment_id='" + sid + "' ORDER BY timestamp DESC LIMIT 1",
            [&oss, &loc_name](int, const char** vals, const char**) {
                if (vals[0]) loc_name = vals[0];
                oss << "Current Location: " << (vals[0] ? vals[0] : "Unknown") << "\n";
                oss << "Last Update:      " << (vals[1] ? vals[1] : "—") << "\n";
                oss << "Source:           " << (vals[2] ? vals[2] : "—") << "\n\n";
            });

        // Disruption events
        db.query("SELECT location,detected_at,type,description FROM disruption_events "
                 "WHERE shipment_id='" + sid + "'",
            [&oss](int, const char** vals, const char**) {
                oss << "⚠ DISRUPTION DETECTED\n";
                oss << "  Location:  " << (vals[0] ? vals[0] : "—") << "\n";
                oss << "  Detected:  " << (vals[1] ? vals[1] : "—") << "\n";
                oss << "  Type:      " << (vals[2] ? vals[2] : "—") << "\n";
                oss << "  Details:   " << (vals[3] ? vals[3] : "—") << "\n\n";
            });

        // Reroutes
        db.query("SELECT from_location,disruption_location,alternate_route,created_at,new_eta "
                 "FROM reroutes WHERE shipment_id='" + sid + "'",
            [&oss](int, const char** vals, const char**) {
                oss << "↪ REROUTE APPLIED\n";
                oss << "  From:            " << (vals[0] ? vals[0] : "—") << "\n";
                oss << "  Disruption at:   " << (vals[1] ? vals[1] : "—") << "\n";
                oss << "  Alternate route: " << (vals[2] ? vals[2] : "—") << "\n";
                oss << "  Rerouted at:     " << (vals[3] ? vals[3] : "—") << "\n";
                oss << "  New ETA:         " << (vals[4] ? vals[4] : "—") << "\n\n";
            });

        // Driver requests
        db.query(R"(SELECT dr.id,d.name,dr.request_type,dr.created_at,dr.status,dr.admin_response
                    FROM driver_requests dr LEFT JOIN drivers d ON dr.driver_id=d.id
                    WHERE dr.shipment_id=')" + sid + "'",
            [&oss](int, const char** vals, const char**) {
                oss << "Driver Request: " << (vals[0] ? vals[0] : "?") << "\n";
                oss << "  Driver:   " << (vals[1] ? vals[1] : "—") << "\n";
                oss << "  Type:     " << (vals[2] ? vals[2] : "—") << "\n";
                oss << "  Submitted:" << (vals[3] ? vals[3] : "—") << "\n";
                oss << "  Status:   " << (vals[4] ? vals[4] : "—") << "\n";
                if (vals[5] && std::string(vals[5]).size() > 0)
                    oss << "  Response: " << vals[5] << "\n";
                oss << "\n";
            });

        answer = oss.str();

    } else if (q.find("driver request") != std::string::npos ||
               q.find("pending request") != std::string::npos ||
               q.find("approved request") != std::string::npos) {
        std::ostringstream oss;
        oss << "Driver Request Summary\n─────────────────────\n\n";
        db.query(R"(SELECT dr.id,d.name,dr.shipment_id,dr.request_type,dr.status,dr.created_at
                    FROM driver_requests dr LEFT JOIN drivers d ON dr.driver_id=d.id
                    ORDER BY dr.created_at DESC)",
            [&oss](int, const char** vals, const char**) {
                std::string st = vals[4] ? vals[4] : "";
                std::string icon = (st == "APPROVED") ? "✓" : (st == "REJECTED") ? "✕" : "⏳";
                oss << icon << " " << (vals[0] ? vals[0] : "?") << "  |  "
                    << (vals[1] ? vals[1] : "?") << "  |  "
                    << (vals[2] ? vals[2] : "?") << "  |  "
                    << (vals[3] ? vals[3] : "?") << "  |  " << st << "\n";
            });
        answer = oss.str();

    } else if (q.find("current location") != std::string::npos ||
               q.find("where is") != std::string::npos ||
               q.find("location of") != std::string::npos) {
        std::ostringstream oss;
        oss << "Last Known Shipment Locations\n─────────────────────────────\n\n";
        db.query("SELECT sl.shipment_id, sl.location_name, sl.timestamp, sl.accuracy_status "
                 "FROM shipment_locations sl "
                 "INNER JOIN (SELECT shipment_id, MAX(timestamp) as mt FROM shipment_locations GROUP BY shipment_id) "
                 "latest ON sl.shipment_id=latest.shipment_id AND sl.timestamp=latest.mt",
            [&oss](int, const char** vals, const char**) {
                oss << "  " << (vals[0]?vals[0]:"?") << ":  " << (vals[1]?vals[1]:"Unknown")
                    << "  (" << (vals[2]?vals[2]:"—") << ")  [" << (vals[3]?vals[3]:"—") << "]\n";
            });
        answer = oss.str();

    } else if (q.find("verification") != std::string::npos ||
               q.find("verified") != std::string::npos ||
               q.find("supported") != std::string::npos) {
        std::ostringstream oss;
        oss << "Disruption Verification Status\n───────────────────────────────\n\n";
        db.query("SELECT de.id, de.shipment_id, de.location, de.type, "
                 "COUNT(vs.id) as sources, SUM(CASE WHEN vs.status='SUPPORTED' THEN 1 ELSE 0 END) as supported "
                 "FROM disruption_events de LEFT JOIN verification_sources vs ON de.id=vs.disruption_event_id "
                 "GROUP BY de.id",
            [&oss](int, const char** vals, const char**) {
                int total = vals[4] ? std::stoi(vals[4]) : 0;
                int supp  = vals[5] ? std::stoi(vals[5]) : 0;
                std::string status = (supp >= 2) ? "MULTI_SOURCE SUPPORTED" :
                                     (supp == 1) ? "SINGLE SOURCE" : "UNVERIFIED";
                oss << "  " << (vals[0]?vals[0]:"?") << " (Shipment " << (vals[1]?vals[1]:"?") << ")\n";
                oss << "  Location: " << (vals[2]?vals[2]:"?") << "  Type: " << (vals[3]?vals[3]:"?") << "\n";
                oss << "  Sources: " << supp << "/" << total << " support this event\n";
                oss << "  Status:  " << status << "\n\n";
            });
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
