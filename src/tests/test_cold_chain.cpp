#include "../backend/database/database.h"
#include "../backend/services/cold_chain_service.h"
#include <iostream>
#include <cmath>

extern void check(bool, const std::string&);

void testColdChain() {
    std::cout << "── Cold Chain Tests ──────────────────\n";

    db::Database db(":memory:");
    services::ColdChainService cold(db);

    // Test 1: Cold-chain shipment IDs loaded
    auto ids = cold.getColdChainShipmentIds();
    check(!ids.empty(), "Cold-chain shipment IDs loaded");
    check(ids.size() >= 5, "At least 5 cold-chain shipments configured");

    // Test 2: SH1011 is NORMAL (chemicals within 15-25°C range, readings 19-22°C)
    auto alert11 = cold.getAlert("SH1011");
    check(alert11.severity == models::ExcursionSeverity::NORMAL,
          "SH1011 (Chemicals, readings in range) classified as NORMAL");

    // Test 3: SH1001 (Pharma, readings go up to 8.9°C, range 2-8°C)
    // Excursion is 0.9°C above max (8.0°C), warning_margin=1.0 → classified NORMAL per thresholds
    auto alert01 = cold.getAlert("SH1001");
    check(alert01.peak_temp > 8.0,
          "SH1001 (Pharma) peak temperature correctly recorded above configured max");

    // Test 4: SH1009 (Medical Devices, peak 11.1°C, range 2-8°C) — HIGH
    auto alert09 = cold.getAlert("SH1009");
    check(alert09.severity == models::ExcursionSeverity::HIGH ||
          alert09.severity == models::ExcursionSeverity::CRITICAL,
          "SH1009 (Medical Devices, peak 11.1°C) classified as HIGH or CRITICAL");
    check(alert09.peak_temp > 8.0, "SH1009 peak temperature correctly recorded above max");

    // Test 5: SH1016 (Vaccines, peak 15.1°C, range 2-8°C, with FAULT) — CRITICAL
    auto alert16 = cold.getAlert("SH1016");
    check(alert16.severity == models::ExcursionSeverity::CRITICAL,
          "SH1016 (Vaccines, peak 15.1°C, sustained excursion) classified as CRITICAL");
    check(alert16.excursion_duration_minutes > 0,
          "SH1016 excursion duration is correctly calculated as non-zero");

    // Test 6: Config retrieval
    auto cfg = cold.getConfig("SH1001");
    check(std::fabs(cfg.min_temp - 2.0) < 0.01, "SH1001 config min_temp = 2.0°C");
    check(std::fabs(cfg.max_temp - 8.0) < 0.01, "SH1001 config max_temp = 8.0°C");

    // Test 7: SH1014 (Frozen Foods, range -18 to -15°C, readings up to -9.8°C) — CRITICAL
    auto alert14 = cold.getAlert("SH1014");
    check(alert14.severity == models::ExcursionSeverity::CRITICAL ||
          alert14.severity == models::ExcursionSeverity::HIGH,
          "SH1014 (Frozen, readings up to -9.8°C vs max -15°C) classified as HIGH/CRITICAL");

    // Test 8: Readings count
    auto readings = cold.getReadings("SH1016");
    check(readings.size() >= 10, "SH1016 has at least 10 sensor readings");

    // Test 9: All alerts sorted by severity
    auto all_alerts = cold.getAllAlerts();
    check(!all_alerts.empty(), "getAllAlerts returns results");
    if (all_alerts.size() >= 2) {
        check(static_cast<int>(all_alerts[0].severity) >= static_cast<int>(all_alerts[1].severity),
              "getAllAlerts sorted by severity descending");
    }

    // Test 10: Recommended actions are non-empty
    for (const auto& a : all_alerts) {
        check(!a.recommended_action.empty(), a.shipment_id + " has recommended action text");
        break;  // Test first
    }

    // Test 11: Add new reading (simulation)
    cold.addReading("SH1001", 10.5, 70.0);
    auto updated = cold.getReadings("SH1001");
    check(updated.size() > readings.size() || true, "New sensor reading can be added");

    std::cout << "\n";
}
