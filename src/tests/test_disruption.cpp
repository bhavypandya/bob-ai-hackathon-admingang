#include "../backend/database/database.h"
#include "../backend/services/disruption_service.h"
#include "../backend/services/shipment_service.h"
#include <iostream>
#include <cassert>

extern void check(bool, const std::string&);

void testDisruption() {
    std::cout << "── Disruption Tests ──────────────────\n";

    db::Database db(":memory:");
    services::DisruptionService dis(db);
    services::ShipmentService ship(db);

    // Test 1: Active disruptions are loaded
    auto active = dis.getActiveDisruptions();
    check(!active.empty(), "Active disruptions loaded from seed data");

    // Test 2: Cyclone disruption is ACTIVE
    bool found_cyclone = false;
    for (const auto& d : active) {
        if (d.name.find("Cyclone") != std::string::npos ||
            d.name.find("Biparjoy") != std::string::npos) {
            found_cyclone = true;
            check(d.severity == models::DisruptionSeverity::CRITICAL,
                  "Cyclone disruption has CRITICAL severity");
            check(!d.affected_routes.empty(),
                  "Cyclone disruption has affected routes");
        }
    }
    check(found_cyclone, "Cyclone Biparjoy disruption exists in seed data");

    // Test 3: Shipments on R01 are affected by cyclone
    auto affected = dis.getAffectedShipmentIds(active[0]);
    check(!affected.empty(), "Disruption on R01 affects at least one shipment");

    // Test 4: Unaffected shipment (R10 - not in any disruption)
    bool r10_in_affected = false;
    for (const auto& id : affected) {
        if (id == "SH1010") { r10_in_affected = true; break; }
    }
    check(!r10_in_affected, "SH1010 (route R10) is not affected by cyclone disruption");

    // Test 5: Impact analysis produces scores
    auto affected_with_scores = ship.getAffectedShipments(active);
    check(!affected_with_scores.empty(), "Affected shipments analysis returns results");

    bool has_critical = false;
    bool has_reasons = false;
    for (const auto& s : affected_with_scores) {
        if (s.impact_score >= 75) has_critical = true;
        if (!s.impact_reasons.empty()) has_reasons = true;
    }
    check(has_critical, "At least one shipment has CRITICAL risk score (>=75)");
    check(has_reasons, "Affected shipments have explanatory impact reasons");

    // Test 6: High-priority cold-chain shipment has elevated score
    for (const auto& s : affected_with_scores) {
        if (s.id == "SH1016") {  // Vaccines, CRITICAL priority, cold chain
            check(s.impact_score >= 50, "SH1016 (vaccine, critical priority) has HIGH+ risk score");
            check(s.cold_chain, "SH1016 is correctly flagged as cold-chain");
            break;
        }
    }

    // Test 7: Set disruption status
    dis.setStatus(1, "RESOLVED");
    auto after = dis.getActiveDisruptions();
    check(after.size() < active.size(), "Setting disruption RESOLVED reduces active count");

    std::cout << "\n";
}
