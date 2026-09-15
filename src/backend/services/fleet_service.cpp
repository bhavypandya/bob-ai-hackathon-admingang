#include "fleet_service.h"
#include <algorithm>
#include <numeric>

namespace services {

FleetService::FleetService(db::Database& db) : db_(db) {}

models::FleetAsset FleetService::rowToAsset(int /*col*/, const char** vals, const char** /*names*/) {
    models::FleetAsset a;
    a.id = vals[0] ? vals[0] : "";
    std::string type = vals[1] ? vals[1] : "";
    if (type == "CONTAINER") a.type = models::AssetType::CONTAINER;
    else if (type == "VESSEL") a.type = models::AssetType::VESSEL;
    else a.type = models::AssetType::TRUCK;

    a.location = vals[2] ? vals[2] : "";

    std::string st = vals[3] ? vals[3] : "";
    if (st == "IN_TRANSIT") a.status = models::AssetStatus::IN_TRANSIT;
    else if (st == "IDLE") a.status = models::AssetStatus::IDLE;
    else if (st == "AVAILABLE") a.status = models::AssetStatus::AVAILABLE;
    else if (st == "MAINTENANCE") a.status = models::AssetStatus::MAINTENANCE;
    else a.status = models::AssetStatus::ACTIVE;

    a.capacity_kg = vals[4] ? std::stoi(vals[4]) : 0;
    a.current_assignment = vals[5] ? vals[5] : "";
    a.availability_time = vals[6] ? vals[6] : "";
    a.utilisation_pct = vals[7] ? std::stoi(vals[7]) : 0;
    a.refrigerated = vals[8] && std::string(vals[8]) == "1";
    return a;
}

std::vector<models::FleetAsset> FleetService::getAllAssets() {
    std::vector<models::FleetAsset> result;
    db_.query("SELECT * FROM fleet_assets ORDER BY id",
        [&](int c, const char** v, const char** n) {
            result.push_back(rowToAsset(c, v, n));
        });
    return result;
}

std::vector<models::FleetAsset> FleetService::getIdleAssets() {
    std::vector<models::FleetAsset> result;
    db_.query("SELECT * FROM fleet_assets WHERE status IN ('IDLE','AVAILABLE') ORDER BY id",
        [&](int c, const char** v, const char** n) {
            result.push_back(rowToAsset(c, v, n));
        });
    return result;
}

FleetUtilisation FleetService::getUtilisation() {
    auto all = getAllAssets();
    FleetUtilisation u{};
    u.total_assets = static_cast<int>(all.size());

    double sum_util = 0;
    for (const auto& a : all) {
        switch (a.status) {
            case models::AssetStatus::ACTIVE:
            case models::AssetStatus::IN_TRANSIT:
                u.active_assets++;
                break;
            case models::AssetStatus::IDLE:
            case models::AssetStatus::AVAILABLE:
                u.idle_assets++;
                break;
            case models::AssetStatus::MAINTENANCE:
                u.maintenance_assets++;
                break;
        }
        sum_util += a.utilisation_pct;
        if (a.utilisation_pct < 30 && a.isIdle()) u.underutilised_assets++;
    }

    u.in_transit_assets = u.active_assets;
    u.average_utilisation_pct = u.total_assets > 0 ? sum_util / u.total_assets : 0.0;

    // Count redeployment opportunities
    auto recs = getRedeploymentRecommendations();
    u.redeployment_opportunities = static_cast<int>(recs.size());

    return u;
}

std::vector<RedeploymentRecommendation> FleetService::getRedeploymentRecommendations() {
    auto idle = getIdleAssets();
    std::vector<RedeploymentRecommendation> recs;

    // Demand hotspots: locations with multiple disrupted shipments
    struct Hotspot { std::string location; int demand; bool needs_refrigeration; };
    std::vector<Hotspot> hotspots;

    // Find locations where shipments are disrupted and fleet is overloaded
    db_.query(R"(
        SELECT destination, COUNT(*) as cnt,
               MAX(cold_chain) as needs_cold
        FROM shipments
        WHERE status IN ('DISRUPTED','AT_RISK','DELAYED')
        GROUP BY destination
        ORDER BY cnt DESC
        LIMIT 5
    )", [&](int, const char** v, const char**) {
        Hotspot h;
        h.location = v[0] ? v[0] : "";
        h.demand = v[1] ? std::stoi(v[1]) : 0;
        h.needs_refrigeration = v[2] && std::string(v[2]) == "1";
        hotspots.push_back(h);
    });

    for (const auto& asset : idle) {
        for (const auto& hs : hotspots) {
            if (asset.location == hs.location) continue;  // already there

            bool compatible = true;
            if (hs.needs_refrigeration && !asset.refrigerated) compatible = false;

            // Don't recommend vessels to non-port locations
            if (asset.type == models::AssetType::VESSEL) {
                // Only recommend to port cities
                bool port_city = (hs.location == "Mumbai" || hs.location == "Chennai" ||
                                  hs.location == "Kolkata" || hs.location == "JNPT");
                if (!port_city) compatible = false;
            }

            if (!compatible) continue;

            RedeploymentRecommendation r;
            r.asset_id = asset.id;
            r.current_location = asset.location;
            r.target_location = hs.location;
            r.asset_type = models::FleetAsset::typeToString(asset.type);
            r.feasible = true;
            r.reason = hs.location + " has " + std::to_string(hs.demand) +
                       " disrupted/delayed shipments and needs additional fleet capacity. " +
                       asset.id + " is currently idle at " + asset.location + ".";

            recs.push_back(r);
            break;  // One recommendation per idle asset
        }
    }

    return recs;
}

void FleetService::updateStatus(const std::string& asset_id, const std::string& status) {
    db_.exec("UPDATE fleet_assets SET status='" + status + "' WHERE id='" + asset_id + "'");
}

} // namespace services
