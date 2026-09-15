#pragma once
#include <string>

namespace models {

enum class AssetType {
    TRUCK,
    CONTAINER,
    VESSEL
};

enum class AssetStatus {
    ACTIVE,
    IN_TRANSIT,
    IDLE,
    AVAILABLE,
    MAINTENANCE
};

struct FleetAsset {
    std::string id;
    AssetType type;
    std::string location;
    AssetStatus status;
    int capacity_kg;
    std::string current_assignment;  // shipment ID or empty
    std::string availability_time;
    int utilisation_pct;             // 0-100
    bool refrigerated;

    // Computed
    std::string recommended_redeployment;
    std::string redeployment_reason;

    static std::string typeToString(AssetType t) {
        switch (t) {
            case AssetType::TRUCK: return "TRUCK";
            case AssetType::CONTAINER: return "CONTAINER";
            case AssetType::VESSEL: return "VESSEL";
        }
        return "UNKNOWN";
    }

    static std::string statusToString(AssetStatus s) {
        switch (s) {
            case AssetStatus::ACTIVE: return "ACTIVE";
            case AssetStatus::IN_TRANSIT: return "IN_TRANSIT";
            case AssetStatus::IDLE: return "IDLE";
            case AssetStatus::AVAILABLE: return "AVAILABLE";
            case AssetStatus::MAINTENANCE: return "MAINTENANCE";
        }
        return "UNKNOWN";
    }

    bool isIdle() const {
        return status == AssetStatus::IDLE || status == AssetStatus::AVAILABLE;
    }
};

} // namespace models
