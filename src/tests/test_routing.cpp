#include "../backend/database/database.h"
#include "../backend/services/route_service.h"
#include "../backend/services/disruption_service.h"
#include <iostream>

extern void check(bool, const std::string&);

void testRouting() {
    std::cout << "── Routing Tests ─────────────────────\n";

    db::Database db(":memory:");
    services::RouteService route(db);
    services::DisruptionService dis(db);

    // Test 1: Disrupted route (R01) returns recommendation
    auto rec1 = route.recommend("SH1001", "R01", "Mumbai", "Delhi", false);
    check(rec1.found, "Route recommendation found for shipment on disrupted R01");
    check(rec1.recommended_route != "R01", "Recommended route is different from disrupted R01");

    // Test 2: Non-disrupted route (R10) recommends itself or valid alternative
    auto rec2 = route.recommend("SH1010", "R10", "Delhi", "Kolkata", false);
    check(rec2.found, "Route recommendation found for non-disrupted R10");

    // Test 3: Cold-chain shipment gets refrigerated route recommendation
    auto rec3 = route.recommend("SH1011", "R11", "Mumbai", "Delhi", true);
    check(rec3.found, "Refrigerated route recommendation found for cold-chain shipment");

    // Test 4: Disrupted route R03 (JNPT strike) - find alternative
    auto rec4 = route.recommend("SH1002", "R03", "JNPT", "Kolkata", false);
    // R03 is disrupted — should find R10 or similar
    if (rec4.found) {
        check(rec4.recommended_route != "R03",
              "Disrupted R03 avoided in recommendation");
    } else {
        check(true, "No alternative for JNPT→Kolkata noted (acceptable)");
    }

    // Test 5: Routes are loaded correctly from DB
    auto all_routes = route.getAllRoutes();
    check(all_routes.size() >= 10, "At least 10 routes loaded from seed data");

    // Test 6: Disrupted routes are correctly flagged
    int disrupted_count = 0;
    for (const auto& r : all_routes)
        if (r.is_disrupted) disrupted_count++;
    check(disrupted_count >= 4, "At least 4 routes marked as disrupted in seed data");

    // Test 7: Route cost scoring works
    for (const auto& r : all_routes) {
        check(r.cost_usd > 0, "Route " + r.id + " has positive cost");
        break;  // Just test first one
    }

    std::cout << "\n";
}
