#pragma once
#include "../database/database.h"
#include "disruption_service.h"
#include "shipment_service.h"
#include "fleet_service.h"
#include "cold_chain_service.h"
#include <string>

namespace services {

struct SimulationResult {
    bool success;
    std::string message;
    int affected_shipments_before;
    int affected_shipments_after;
    int idle_assets_before;
    int idle_assets_after;
    int cold_chain_alerts_before;
    int cold_chain_alerts_after;
};

class SimulationService {
public:
    SimulationService(db::Database& db,
                      DisruptionService& dis,
                      ShipmentService& ship,
                      FleetService& fleet,
                      ColdChainService& cold);

    // Apply a simulation scenario by name
    SimulationResult applyScenario(const std::string& scenario_type,
                                   const std::string& target = "");

    // Reset to baseline state
    void reset();

private:
    db::Database& db_;
    DisruptionService& dis_;
    ShipmentService& ship_;
    FleetService& fleet_;
    ColdChainService& cold_;

    void applyWeatherDisruption();
    void applyRoadClosure();
    void applyPortStrike();
    void applyCarrierUnavailable(const std::string& carrier_id);
    void applyFleetUnavailable(const std::string& asset_id);
    void applyDemandIncrease();
    void applyTemperatureExcursion(const std::string& shipment_id);
};

} // namespace services
