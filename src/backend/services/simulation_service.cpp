#include "simulation_service.h"
#include <ctime>
#include <string>

namespace services {

SimulationService::SimulationService(
    db::Database& db, DisruptionService& dis, ShipmentService& ship,
    FleetService& fleet, ColdChainService& cold)
    : db_(db), dis_(dis), ship_(ship), fleet_(fleet), cold_(cold) {}

static std::string nowStr() {
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&now));
    return buf;
}

static std::string futureStr(int hours) {
    std::time_t t = std::time(nullptr) + hours * 3600;
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&t));
    return buf;
}

SimulationResult SimulationService::applyScenario(const std::string& scenario_type,
                                                    const std::string& target) {
    // Snapshot before
    auto disruptions_before = dis_.getActiveDisruptions();
    auto affected_before = ship_.getAffectedShipments(disruptions_before);
    auto idle_before = fleet_.getIdleAssets();
    auto cold_before = cold_.getAllAlerts();
    int cold_issues_before = 0;
    for (const auto& a : cold_before)
        if (a.severity != models::ExcursionSeverity::NORMAL) cold_issues_before++;

    SimulationResult result;
    result.success = true;
    result.affected_shipments_before = static_cast<int>(affected_before.size());
    result.idle_assets_before = static_cast<int>(idle_before.size());
    result.cold_chain_alerts_before = cold_issues_before;

    if (scenario_type == "weather_disruption") {
        applyWeatherDisruption();
        result.message = "Weather disruption scenario applied. New cyclone event added affecting R05 and R13.";
    } else if (scenario_type == "road_closure") {
        applyRoadClosure();
        result.message = "Road closure scenario applied. Delhi–Jaipur highway (R06) is now closed.";
    } else if (scenario_type == "port_strike") {
        applyPortStrike();
        result.message = "Port strike scenario applied. Chennai port (R09, R14) disrupted.";
    } else if (scenario_type == "carrier_unavailable") {
        std::string cid = target.empty() ? "C07" : target;
        applyCarrierUnavailable(cid);
        result.message = "Carrier " + cid + " marked as unavailable (fleet recall).";
    } else if (scenario_type == "fleet_unavailable") {
        std::string aid = target.empty() ? "T215" : target;
        applyFleetUnavailable(aid);
        result.message = "Fleet asset " + aid + " moved to MAINTENANCE status.";
    } else if (scenario_type == "demand_increase") {
        applyDemandIncrease();
        result.message = "Demand surge applied. 5 additional high-priority shipments added to Mumbai–Delhi route.";
    } else if (scenario_type == "temperature_excursion") {
        std::string sid = target.empty() ? "SH1016" : target;
        applyTemperatureExcursion(sid);
        result.message = "Temperature excursion injected for " + sid + " — readings exceed configured range.";
    } else if (scenario_type == "reset") {
        reset();
        result.message = "Simulation reset to baseline state.";
    } else {
        result.success = false;
        result.message = "Unknown scenario type: " + scenario_type;
    }

    // Snapshot after
    auto disruptions_after = dis_.getActiveDisruptions();
    auto affected_after = ship_.getAffectedShipments(disruptions_after);
    auto idle_after = fleet_.getIdleAssets();
    auto cold_after = cold_.getAllAlerts();
    int cold_issues_after = 0;
    for (const auto& a : cold_after)
        if (a.severity != models::ExcursionSeverity::NORMAL) cold_issues_after++;

    result.affected_shipments_after = static_cast<int>(affected_after.size());
    result.idle_assets_after = static_cast<int>(idle_after.size());
    result.cold_chain_alerts_after = cold_issues_after;

    return result;
}

void SimulationService::reset() {
    db_.resetToSeedState();
}

void SimulationService::applyWeatherDisruption() {
    models::Disruption d;
    d.type = models::DisruptionType::WEATHER_EVENT;
    d.name = "Simulated Storm — Gujarat Coast";
    d.location = "Ahmedabad / Gujarat";
    d.severity = models::DisruptionSeverity::HIGH;
    d.start_time = nowStr();
    d.expected_end_time = futureStr(48);
    d.affected_routes = {"R05", "R13"};
    d.description = "Simulated weather event. Heavy rainfall and high winds disrupting freight on NH-48 Gujarat section.";
    d.status = models::DisruptionStatus::ACTIVE;
    int new_id = dis_.addDisruption(d);

    dis_.setRouteDisrupted("R05", true, "Simulated weather disruption");
    dis_.setRouteDisrupted("R13", true, "Simulated weather disruption");

    // Update affected shipments
    db_.exec("UPDATE shipments SET status='DISRUPTED', expected_delay_hours=expected_delay_hours+18 WHERE current_route IN ('R05','R13') AND status != 'DELIVERED'");
}

void SimulationService::applyRoadClosure() {
    models::Disruption d;
    d.type = models::DisruptionType::ROAD_CLOSURE;
    d.name = "Simulated Road Closure — Jaipur Bypass";
    d.location = "Delhi–Jaipur NH-48";
    d.severity = models::DisruptionSeverity::MEDIUM;
    d.start_time = nowStr();
    d.expected_end_time = futureStr(12);
    d.affected_routes = {"R06"};
    d.description = "Simulated road closure due to accident. All freight traffic diverted.";
    d.status = models::DisruptionStatus::ACTIVE;
    dis_.addDisruption(d);

    dis_.setRouteDisrupted("R06", true, "Simulated road closure");
    db_.exec("UPDATE shipments SET status='DELAYED', expected_delay_hours=expected_delay_hours+6 WHERE current_route='R06' AND status != 'DELIVERED'");
}

void SimulationService::applyPortStrike() {
    models::Disruption d;
    d.type = models::DisruptionType::PORT_STRIKE;
    d.name = "Simulated Chennai Port Strike";
    d.location = "Chennai Port";
    d.severity = models::DisruptionSeverity::HIGH;
    d.start_time = nowStr();
    d.expected_end_time = futureStr(72);
    d.affected_routes = {"R09", "R14"};
    d.description = "Simulated labour strike at Chennai Port. All container handling suspended.";
    d.status = models::DisruptionStatus::ACTIVE;
    dis_.addDisruption(d);

    dis_.setRouteDisrupted("R09", true, "Simulated port strike");
    dis_.setRouteDisrupted("R14", true, "Simulated port strike");
    db_.exec("UPDATE shipments SET status='DISRUPTED', expected_delay_hours=expected_delay_hours+48 WHERE current_route IN ('R09','R14') AND status != 'DELIVERED'");
}

void SimulationService::applyCarrierUnavailable(const std::string& carrier_id) {
    dis_.setCarrierDisrupted(carrier_id, true, "Simulated fleet recall");
    db_.exec("UPDATE shipments SET status='AT_RISK' WHERE carrier='" + carrier_id + "' AND status='ON_TIME'");
}

void SimulationService::applyFleetUnavailable(const std::string& asset_id) {
    db_.exec("UPDATE fleet_assets SET status='MAINTENANCE', utilisation_pct=0 WHERE id='" + asset_id + "'");
}

void SimulationService::applyDemandIncrease() {
    // Add 5 new high-priority shipments
    db_.exec(R"(
        INSERT OR IGNORE INTO shipments VALUES
        ('SH2001','Mumbai','Delhi','R01','C01','','DISRUPTED','HIGH','Consumer Electronics',0,'2024-06-10 14:00','2024-06-11 08:00','2024-06-12 18:00',34),
        ('SH2002','Mumbai','Delhi','R01','C07','','DISRUPTED','HIGH','Medical Supplies',1,'2024-06-10 15:00','2024-06-11 09:00','2024-06-12 19:00',34),
        ('SH2003','Mumbai','Delhi','R11','C05','','ON_TIME','CRITICAL','Vaccines',1,'2024-06-10 14:00','2024-06-11 14:00','2024-06-11 14:00',0),
        ('SH2004','Mumbai','Delhi','R01','C02','','DISRUPTED','MEDIUM','Spare Parts',0,'2024-06-10 16:00','2024-06-11 10:00','2024-06-12 20:00',34),
        ('SH2005','Mumbai','Delhi','R11','C05','','ON_TIME','HIGH','Chemicals',0,'2024-06-10 13:00','2024-06-11 13:00','2024-06-11 13:00',0)
    )");
}

void SimulationService::applyTemperatureExcursion(const std::string& shipment_id) {
    // Inject a series of out-of-range readings
    db_.exec("DELETE FROM sensor_readings WHERE shipment_id='" + shipment_id + "'");
    db_.exec(R"(
        INSERT INTO sensor_readings (shipment_id,timestamp,temperature,humidity,sensor_status) VALUES
        (')" + shipment_id + R"(','2024-06-10 05:00',4.2,62,'OK'),
        (')" + shipment_id + R"(','2024-06-10 05:30',5.5,64,'OK'),
        (')" + shipment_id + R"(','2024-06-10 06:00',8.9,66,'OK'),
        (')" + shipment_id + R"(','2024-06-10 06:30',12.5,68,'OK'),
        (')" + shipment_id + R"(','2024-06-10 07:00',16.8,72,'FAULT'),
        (')" + shipment_id + R"(','2024-06-10 07:30',18.4,74,'FAULT'),
        (')" + shipment_id + R"(','2024-06-10 08:00',17.2,73,'FAULT'),
        (')" + shipment_id + R"(','2024-06-10 08:30',15.6,71,'OK'),
        (')" + shipment_id + R"(','2024-06-10 09:00',13.1,69,'OK'),
        (')" + shipment_id + R"(','2024-06-10 09:30',10.5,67,'OK'),
        (')" + shipment_id + R"(','2024-06-10 10:00',8.2,65,'OK'),
        (')" + shipment_id + R"(','2024-06-10 10:30',6.5,63,'OK')
    )");
}

} // namespace services
