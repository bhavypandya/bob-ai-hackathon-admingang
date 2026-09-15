#pragma once
#include <string>
#include <vector>

namespace models {

enum class ExcursionSeverity {
    NORMAL,
    WARNING,
    HIGH,
    CRITICAL
};

struct SensorReading {
    std::string shipment_id;
    std::string timestamp;
    double temperature;
    double humidity;
    std::string sensor_status;   // OK / FAULT / OFFLINE
};

struct ColdChainConfig {
    double min_temp;
    double max_temp;
    double warning_margin;    // degrees outside range to trigger WARNING
    double critical_margin;   // degrees outside range to trigger CRITICAL
    int allowed_excursion_minutes;
};

struct ColdChainAlert {
    std::string shipment_id;
    double current_temp;
    double peak_temp;
    double min_temp_recorded;
    double max_temp_recorded;
    int excursion_duration_minutes;
    int excursion_count;
    int minutes_to_delivery;
    ExcursionSeverity severity;
    std::string severity_reason;
    std::string recommended_action;

    ColdChainConfig config;
    std::vector<SensorReading> readings;

    static std::string severityToString(ExcursionSeverity s) {
        switch (s) {
            case ExcursionSeverity::NORMAL: return "NORMAL";
            case ExcursionSeverity::WARNING: return "WARNING";
            case ExcursionSeverity::HIGH: return "HIGH";
            case ExcursionSeverity::CRITICAL: return "CRITICAL";
        }
        return "NORMAL";
    }
};

} // namespace models
