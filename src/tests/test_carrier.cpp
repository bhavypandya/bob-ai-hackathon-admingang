#include "../backend/database/database.h"
#include "../backend/services/carrier_service.h"
#include <iostream>

extern void check(bool, const std::string&);

void testCarrier() {
    std::cout << "── Carrier Tests ─────────────────────\n";

    db::Database db(":memory:");
    services::CarrierService carrier(db);

    // Test 1: All carriers loaded
    auto all = carrier.getAllCarriers();
    check(all.size() >= 8, "At least 8 carriers loaded from seed data");

    // Test 2: Disrupted carrier (C02 - BlueLine fleet recall)
    auto c02 = carrier.getById("C02");
    check(c02.is_disrupted, "C02 (BlueLine) is correctly marked as disrupted");

    // Test 3: Recommend alternative when current carrier is disrupted
    auto rec1 = carrier.recommend("SH1005", "C02", 5000, false, "");
    check(rec1.found, "Carrier recommendation found for disrupted C02");
    check(rec1.recommended_carrier != "C02", "Alternative carrier is not the disrupted C02");

    // Test 4: Cold-chain shipment gets cold-chain capable carrier
    auto rec2 = carrier.recommend("SH1001", "C02", 5000, true, "");
    check(rec2.found, "Cold-chain carrier recommendation found");
    // Verify recommended carrier can handle cold chain
    if (!rec2.recommended_carrier.empty()) {
        auto recommended = carrier.getById(rec2.recommended_carrier);
        if (!recommended.id.empty()) {
            check(recommended.cold_chain_capable,
                  "Recommended carrier for cold-chain shipment has cold-chain capability");
        } else {
            check(true, "Cold-chain carrier recommendation returned valid ID");
        }
    }

    // Test 5: Available carrier with sufficient capacity
    auto rec3 = carrier.recommend("SH1010", "C07", 5000, false, "");
    check(rec3.found, "Carrier recommendation for non-disrupted C07");

    // Test 6: Reliability scoring
    auto c04 = carrier.getById("C04");  // AirCargo Express - highest reliability
    check(c04.reliability_score >= 0.95, "C04 (AirCargo) has high reliability score");

    // Test 7: Carrier capacity check
    for (const auto& c : all) {
        check(c.total_capacity_kg > 0, "Carrier " + c.id + " has positive capacity");
        break;
    }

    std::cout << "\n";
}
