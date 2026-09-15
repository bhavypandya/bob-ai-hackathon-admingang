#pragma once
#include <string>
#include <vector>

namespace models {

enum class ShipmentStatus {
    ON_TIME,
    DELAYED,
    AT_RISK,
    DISRUPTED,
    DELIVERED
};

enum class ShipmentPriority {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

struct Shipment {
    std::string id;
    std::string origin;
    std::string destination;
    std::string current_route;      // route ID
    std::string carrier;            // carrier ID
    std::string fleet_asset;        // asset ID
    ShipmentStatus status;
    ShipmentPriority priority;
    std::string cargo_type;
    bool cold_chain;
    std::string planned_departure;
    std::string planned_arrival;
    std::string current_eta;
    int expected_delay_hours;       // hours

    // Computed fields (set by analysis)
    int impact_score;               // 0-100
    bool is_affected;
    std::string risk_level;         // LOW/MEDIUM/HIGH/CRITICAL
    std::vector<std::string> impact_reasons;
    std::string recommended_route;
    std::string recommended_carrier;

    static std::string statusToString(ShipmentStatus s) {
        switch (s) {
            case ShipmentStatus::ON_TIME: return "ON_TIME";
            case ShipmentStatus::DELAYED: return "DELAYED";
            case ShipmentStatus::AT_RISK: return "AT_RISK";
            case ShipmentStatus::DISRUPTED: return "DISRUPTED";
            case ShipmentStatus::DELIVERED: return "DELIVERED";
        }
        return "UNKNOWN";
    }

    static std::string priorityToString(ShipmentPriority p) {
        switch (p) {
            case ShipmentPriority::LOW: return "LOW";
            case ShipmentPriority::MEDIUM: return "MEDIUM";
            case ShipmentPriority::HIGH: return "HIGH";
            case ShipmentPriority::CRITICAL: return "CRITICAL";
        }
        return "UNKNOWN";
    }

    static std::string riskLabel(int score) {
        if (score >= 75) return "CRITICAL";
        if (score >= 50) return "HIGH";
        if (score >= 25) return "MEDIUM";
        return "LOW";
    }
};

} // namespace models
