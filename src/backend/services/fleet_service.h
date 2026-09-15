#pragma once
#include "../models/fleet_asset.h"
#include "../database/database.h"
#include <vector>
#include <string>

namespace services {

struct FleetUtilisation {
    int total_assets;
    int active_assets;
    int idle_assets;
    int in_transit_assets;
    int maintenance_assets;
    double average_utilisation_pct;
    int underutilised_assets;  // utilisation < 30%
    int redeployment_opportunities;
};

struct RedeploymentRecommendation {
    std::string asset_id;
    std::string current_location;
    std::string target_location;
    std::string reason;
    std::string asset_type;
    bool feasible;
};

class FleetService {
public:
    explicit FleetService(db::Database& db);

    std::vector<models::FleetAsset> getAllAssets();
    std::vector<models::FleetAsset> getIdleAssets();
    FleetUtilisation getUtilisation();
    std::vector<RedeploymentRecommendation> getRedeploymentRecommendations();

    // Update asset status (simulation)
    void updateStatus(const std::string& asset_id, const std::string& status);

private:
    db::Database& db_;
    models::FleetAsset rowToAsset(int col, const char** vals, const char** names);
};

} // namespace services
