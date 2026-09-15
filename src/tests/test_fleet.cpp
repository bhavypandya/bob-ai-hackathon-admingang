#include "../backend/database/database.h"
#include "../backend/services/fleet_service.h"
#include <iostream>

extern void check(bool, const std::string&);

void testFleet() {
    std::cout << "── Fleet Tests ───────────────────────\n";

    db::Database db(":memory:");
    services::FleetService fleet(db);

    // Test 1: All assets loaded
    auto all = fleet.getAllAssets();
    check(all.size() >= 20, "At least 20 fleet assets loaded from seed data");

    // Test 2: Idle assets detected
    auto idle = fleet.getIdleAssets();
    check(!idle.empty(), "Idle assets detected in fleet");
    check(idle.size() >= 3, "At least 3 idle assets in seed data");

    // Test 3: Idle assets have correct status
    for (const auto& a : idle) {
        bool is_idle = (a.status == models::AssetStatus::IDLE ||
                        a.status == models::AssetStatus::AVAILABLE);
        check(is_idle, "Idle asset " + a.id + " has IDLE or AVAILABLE status");
    }

    // Test 4: Fleet utilisation metrics
    auto util = fleet.getUtilisation();
    check(util.total_assets > 0, "Fleet utilisation total_assets > 0");
    check(util.active_assets > 0, "Fleet has active/in-transit assets");
    check(util.idle_assets > 0, "Fleet has idle assets");
    check(util.average_utilisation_pct >= 0 && util.average_utilisation_pct <= 100,
          "Average utilisation is in range 0–100%");

    // Test 5: Redeployment recommendations generated
    auto recs = fleet.getRedeploymentRecommendations();
    check(!recs.empty(), "Redeployment opportunities identified");
    for (const auto& r : recs) {
        check(!r.asset_id.empty(), "Redeployment recommendation has valid asset ID");
        check(!r.target_location.empty(), "Redeployment recommendation has target location");
        check(r.current_location != r.target_location,
              "Redeployment target differs from current location for " + r.asset_id);
        break;  // Test first recommendation
    }

    // Test 6: T204 (IDLE at Mumbai) is detected
    bool found_t204 = false;
    for (const auto& a : idle) {
        if (a.id == "T204") { found_t204 = true; break; }
    }
    check(found_t204, "T204 (explicitly IDLE in seed data) detected as idle");

    // Test 7: Refrigerated assets identified
    bool has_refrigerated = false;
    for (const auto& a : all) {
        if (a.refrigerated) { has_refrigerated = true; break; }
    }
    check(has_refrigerated, "Refrigerated fleet assets exist in seed data");

    // Test 8: Update status
    fleet.updateStatus("T204", "IN_TRANSIT");
    auto idle_after = fleet.getIdleAssets();
    bool t204_still_idle = false;
    for (const auto& a : idle_after) {
        if (a.id == "T204") { t204_still_idle = true; break; }
    }
    check(!t204_still_idle, "T204 removed from idle after status update");

    std::cout << "\n";
}
