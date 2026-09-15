#include "cold_chain_service.h"
#include <algorithm>
#include <cmath>
#include <ctime>

namespace services {

ColdChainService::ColdChainService(db::Database& db) : db_(db) {}

std::vector<std::string> ColdChainService::getColdChainShipmentIds() {
    std::vector<std::string> ids;
    db_.query("SELECT DISTINCT shipment_id FROM cold_chain_config",
        [&](int, const char** v, const char**) {
            if (v[0]) ids.push_back(v[0]);
        });
    return ids;
}

models::ColdChainConfig ColdChainService::getConfig(const std::string& shipment_id) {
    models::ColdChainConfig cfg{};
    cfg.min_temp = 2.0;
    cfg.max_temp = 8.0;
    cfg.warning_margin = 1.0;
    cfg.critical_margin = 3.0;
    cfg.allowed_excursion_minutes = 30;

    db_.query("SELECT * FROM cold_chain_config WHERE shipment_id='" + shipment_id + "'",
        [&](int, const char** v, const char**) {
            cfg.min_temp = v[1] ? std::stod(v[1]) : 2.0;
            cfg.max_temp = v[2] ? std::stod(v[2]) : 8.0;
            cfg.warning_margin = v[3] ? std::stod(v[3]) : 1.0;
            cfg.critical_margin = v[4] ? std::stod(v[4]) : 3.0;
            cfg.allowed_excursion_minutes = v[5] ? std::stoi(v[5]) : 30;
        });
    return cfg;
}

std::vector<models::SensorReading> ColdChainService::getReadings(const std::string& shipment_id) {
    std::vector<models::SensorReading> readings;
    db_.query("SELECT shipment_id,timestamp,temperature,humidity,sensor_status FROM sensor_readings "
              "WHERE shipment_id='" + shipment_id + "' ORDER BY timestamp",
        [&](int, const char** v, const char**) {
            models::SensorReading r;
            r.shipment_id = v[0] ? v[0] : "";
            r.timestamp = v[1] ? v[1] : "";
            r.temperature = v[2] ? std::stod(v[2]) : 0.0;
            r.humidity = v[3] ? std::stod(v[3]) : 0.0;
            r.sensor_status = v[4] ? v[4] : "OK";
            readings.push_back(r);
        });
    return readings;
}

models::ColdChainAlert ColdChainService::getAlert(const std::string& shipment_id) {
    models::ColdChainAlert alert;
    alert.shipment_id = shipment_id;
    alert.severity = models::ExcursionSeverity::NORMAL;

    auto cfg = getConfig(shipment_id);
    alert.config = cfg;

    auto readings = getReadings(shipment_id);
    alert.readings = readings;

    if (readings.empty()) {
        alert.severity_reason = "No sensor data available.";
        alert.recommended_action = "Check sensor connectivity.";
        return alert;
    }

    // Calculate stats
    double current = readings.back().temperature;
    double peak = readings[0].temperature;
    double min_r = readings[0].temperature;
    double max_r = readings[0].temperature;

    for (const auto& r : readings) {
        if (r.temperature > peak) peak = r.temperature;
        if (r.temperature < min_r) min_r = r.temperature;
        if (r.temperature > max_r) max_r = r.temperature;
    }

    alert.current_temp = current;
    alert.peak_temp = peak;
    alert.min_temp_recorded = min_r;
    alert.max_temp_recorded = max_r;

    // Count excursion duration (consecutive readings outside range × 30 min each)
    int excursion_minutes = 0;
    int excursion_count = 0;
    bool in_excursion = false;
    int current_excursion = 0;

    for (const auto& r : readings) {
        bool outside = (r.temperature < cfg.min_temp || r.temperature > cfg.max_temp);
        if (outside) {
            if (!in_excursion) {
                excursion_count++;
                in_excursion = true;
                current_excursion = 0;
            }
            current_excursion += 30;  // assume 30-min intervals
        } else {
            if (in_excursion) {
                excursion_minutes += current_excursion;
                in_excursion = false;
            }
        }
    }
    if (in_excursion) excursion_minutes += current_excursion;

    alert.excursion_duration_minutes = excursion_minutes;
    alert.excursion_count = excursion_count;

    // Time to delivery: query the shipment
    int minutes_to_delivery = 0;
    db_.query("SELECT planned_arrival FROM shipments WHERE id='" + shipment_id + "'",
        [&](int, const char** v, const char**) {
            // Simplified: use expected_delay_hours to estimate
        });
    db_.query("SELECT expected_delay_hours FROM shipments WHERE id='" + shipment_id + "'",
        [&](int, const char** v, const char**) {
            int delay = v[0] ? std::stoi(v[0]) : 0;
            minutes_to_delivery = 180 + delay * 60;  // simplified
        });
    alert.minutes_to_delivery = minutes_to_delivery;

    // Classify
    alert.severity = classify(cfg, peak, excursion_minutes, excursion_count, minutes_to_delivery);

    // Build severity reason
    std::string sev_str = models::ColdChainAlert::severityToString(alert.severity);
    // Format temperatures with 1 decimal place
    auto fmtTemp = [](double t) -> std::string {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.1f", t);
        return buf;
    };

    alert.severity_reason = "Configured range: " + fmtTemp(cfg.min_temp) + "°C – " +
                            fmtTemp(cfg.max_temp) + "°C. " +
                            "Peak recorded: " + fmtTemp(peak) + "°C. ";
    if (excursion_minutes > 0) {
        alert.severity_reason += "Excursion duration: " + std::to_string(excursion_minutes) + " min. ";
    }
    if (excursion_count > 1) {
        alert.severity_reason += std::to_string(excursion_count) + " separate excursion events detected.";
    }

    alert.recommended_action = buildRecommendedAction(alert.severity, shipment_id);
    return alert;
}

std::vector<models::ColdChainAlert> ColdChainService::getAllAlerts() {
    std::vector<models::ColdChainAlert> alerts;
    auto ids = getColdChainShipmentIds();
    for (const auto& id : ids) {
        alerts.push_back(getAlert(id));
    }
    // Sort by severity descending
    std::sort(alerts.begin(), alerts.end(),
        [](const models::ColdChainAlert& a, const models::ColdChainAlert& b) {
            return static_cast<int>(a.severity) > static_cast<int>(b.severity);
        });
    return alerts;
}

models::ExcursionSeverity ColdChainService::classify(
    const models::ColdChainConfig& cfg,
    double peak_temp,
    int excursion_minutes,
    int excursion_count,
    int minutes_to_delivery) {

    if (excursion_minutes == 0) return models::ExcursionSeverity::NORMAL;

    // How far outside the range?
    double excursion_magnitude = 0.0;
    if (peak_temp > cfg.max_temp) excursion_magnitude = peak_temp - cfg.max_temp;
    else if (peak_temp < cfg.min_temp) excursion_magnitude = cfg.min_temp - peak_temp;

    // CRITICAL conditions
    if (excursion_magnitude >= cfg.critical_margin) return models::ExcursionSeverity::CRITICAL;
    if (excursion_minutes >= cfg.allowed_excursion_minutes * 3) return models::ExcursionSeverity::CRITICAL;
    if (excursion_count >= 3 && excursion_minutes > cfg.allowed_excursion_minutes)
        return models::ExcursionSeverity::CRITICAL;
    // Urgent: excursion ongoing when close to delivery
    if (minutes_to_delivery < 120 && excursion_minutes > cfg.allowed_excursion_minutes)
        return models::ExcursionSeverity::CRITICAL;

    // HIGH conditions
    if (excursion_magnitude >= cfg.warning_margin * 2) return models::ExcursionSeverity::HIGH;
    if (excursion_minutes >= cfg.allowed_excursion_minutes) return models::ExcursionSeverity::HIGH;
    if (excursion_count >= 2) return models::ExcursionSeverity::HIGH;

    // WARNING
    if (excursion_magnitude >= cfg.warning_margin) return models::ExcursionSeverity::WARNING;
    if (excursion_minutes > 0) return models::ExcursionSeverity::WARNING;

    return models::ExcursionSeverity::NORMAL;
}

std::string ColdChainService::buildRecommendedAction(models::ExcursionSeverity sev,
                                                      const std::string& shipment_id) {
    switch (sev) {
        case models::ExcursionSeverity::NORMAL:
            return "No action required. Continue standard monitoring.";
        case models::ExcursionSeverity::WARNING:
            return "Monitor " + shipment_id + " closely. Review against applicable product handling requirements. Notify carrier to inspect refrigeration unit.";
        case models::ExcursionSeverity::HIGH:
            return "Priority cold-chain review required for " + shipment_id + ". Contact carrier immediately. Review against applicable product handling requirements. Consider expedited delivery.";
        case models::ExcursionSeverity::CRITICAL:
            return "URGENT: Immediate intervention required for " + shipment_id + ". Halt delivery if possible. Review against applicable product handling requirements. Escalate to quality team and consignee.";
    }
    return "Review against applicable product handling requirements.";
}

void ColdChainService::addReading(const std::string& shipment_id, double temperature, double humidity) {
    // Get current timestamp (simplified)
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", std::localtime(&now));

    db_.exec("INSERT INTO sensor_readings (shipment_id,timestamp,temperature,humidity,sensor_status) VALUES ('"
        + shipment_id + "','" + std::string(buf) + "',"
        + std::to_string(temperature) + "," + std::to_string(humidity) + ",'OK')");
}

} // namespace services
