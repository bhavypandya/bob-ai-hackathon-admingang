#pragma once
#include <string>
#include <vector>

namespace models {

enum class DisruptionType {
    WEATHER_EVENT,
    PORT_STRIKE,
    GEOPOLITICAL_CRISIS,
    ROAD_CLOSURE,
    CARRIER_DISRUPTION
};

enum class DisruptionSeverity {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

enum class DisruptionStatus {
    ACTIVE,
    MONITORING,
    RESOLVED
};

struct Disruption {
    int id;
    DisruptionType type;
    std::string name;
    std::string location;
    DisruptionSeverity severity;
    std::string start_time;
    std::string expected_end_time;
    std::vector<std::string> affected_routes;  // route IDs as strings
    std::string description;
    DisruptionStatus status;

    static std::string typeToString(DisruptionType t) {
        switch (t) {
            case DisruptionType::WEATHER_EVENT: return "Weather Event";
            case DisruptionType::PORT_STRIKE: return "Port Strike";
            case DisruptionType::GEOPOLITICAL_CRISIS: return "Geopolitical Crisis";
            case DisruptionType::ROAD_CLOSURE: return "Road Closure";
            case DisruptionType::CARRIER_DISRUPTION: return "Carrier Disruption";
        }
        return "Unknown";
    }

    static std::string severityToString(DisruptionSeverity s) {
        switch (s) {
            case DisruptionSeverity::LOW: return "LOW";
            case DisruptionSeverity::MEDIUM: return "MEDIUM";
            case DisruptionSeverity::HIGH: return "HIGH";
            case DisruptionSeverity::CRITICAL: return "CRITICAL";
        }
        return "UNKNOWN";
    }

    static std::string statusToString(DisruptionStatus s) {
        switch (s) {
            case DisruptionStatus::ACTIVE: return "ACTIVE";
            case DisruptionStatus::MONITORING: return "MONITORING";
            case DisruptionStatus::RESOLVED: return "RESOLVED";
        }
        return "UNKNOWN";
    }

    static DisruptionSeverity severityFromInt(int v) {
        if (v >= 75) return DisruptionSeverity::CRITICAL;
        if (v >= 50) return DisruptionSeverity::HIGH;
        if (v >= 25) return DisruptionSeverity::MEDIUM;
        return DisruptionSeverity::LOW;
    }
};

} // namespace models
