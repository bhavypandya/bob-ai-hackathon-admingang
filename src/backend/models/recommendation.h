#pragma once
#include <string>

namespace models {

enum class RecommendationType {
    REROUTE_SHIPMENT,
    CHANGE_CARRIER,
    REDEPLOY_FLEET,
    PRIORITISE_SHIPMENT,
    COLD_CHAIN_REVIEW,
    MONITOR_DISRUPTION
};

enum class RecommendationPriority {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

struct Recommendation {
    int id;
    RecommendationType type;
    RecommendationPriority priority;
    std::string target;          // shipment/asset/carrier ID
    std::string reason;
    std::string expected_impact;
    std::string action;          // human-readable action text

    static std::string typeToString(RecommendationType t) {
        switch (t) {
            case RecommendationType::REROUTE_SHIPMENT: return "REROUTE_SHIPMENT";
            case RecommendationType::CHANGE_CARRIER: return "CHANGE_CARRIER";
            case RecommendationType::REDEPLOY_FLEET: return "REDEPLOY_FLEET";
            case RecommendationType::PRIORITISE_SHIPMENT: return "PRIORITISE_SHIPMENT";
            case RecommendationType::COLD_CHAIN_REVIEW: return "COLD_CHAIN_REVIEW";
            case RecommendationType::MONITOR_DISRUPTION: return "MONITOR_DISRUPTION";
        }
        return "UNKNOWN";
    }

    static std::string priorityToString(RecommendationPriority p) {
        switch (p) {
            case RecommendationPriority::LOW: return "LOW";
            case RecommendationPriority::MEDIUM: return "MEDIUM";
            case RecommendationPriority::HIGH: return "HIGH";
            case RecommendationPriority::CRITICAL: return "CRITICAL";
        }
        return "UNKNOWN";
    }
};

} // namespace models
