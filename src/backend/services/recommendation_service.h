#pragma once
#include "../models/recommendation.h"
#include "../database/database.h"
#include "disruption_service.h"
#include "shipment_service.h"
#include "route_service.h"
#include "carrier_service.h"
#include "fleet_service.h"
#include "cold_chain_service.h"
#include <vector>

namespace services {

class RecommendationService {
public:
    RecommendationService(db::Database& db,
                          DisruptionService& dis,
                          ShipmentService& ship,
                          RouteService& route,
                          CarrierService& carrier,
                          FleetService& fleet,
                          ColdChainService& cold);

    // Generate all current recommendations based on live data
    std::vector<models::Recommendation> generateAll();

    // Generate summary text suitable for Bob
    std::string generateBobSummary();

private:
    db::Database& db_;
    DisruptionService& dis_;
    ShipmentService& ship_;
    RouteService& route_;
    CarrierService& carrier_;
    FleetService& fleet_;
    ColdChainService& cold_;
};

} // namespace services
