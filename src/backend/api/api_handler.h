#pragma once
#include "../database/database.h"
#include "../services/disruption_service.h"
#include "../services/shipment_service.h"
#include "../services/route_service.h"
#include "../services/carrier_service.h"
#include "../services/fleet_service.h"
#include "../services/cold_chain_service.h"
#include "../services/recommendation_service.h"
#include "../services/simulation_service.h"
#include <string>

namespace api {

// All handler functions take request body (string) and return JSON string
// The actual HTTP routing is done in main.cpp using Crow

std::string handleHealth();
std::string handleDashboard(
    db::Database& db,
    services::DisruptionService& dis,
    services::ShipmentService& ship,
    services::FleetService& fleet,
    services::ColdChainService& cold,
    services::RecommendationService& rec_svc);

std::string handleGetShipments(services::ShipmentService& ship);
std::string handleGetAffectedShipments(
    services::ShipmentService& ship,
    services::DisruptionService& dis);

std::string handleGetDisruptions(services::DisruptionService& dis);

std::string handleGetRoutes(services::RouteService& route);
std::string handlePostRouteRecommend(
    const std::string& body,
    services::ShipmentService& ship,
    services::RouteService& route);

std::string handleGetCarriers(services::CarrierService& carrier);
std::string handlePostCarrierRecommend(
    const std::string& body,
    services::ShipmentService& ship,
    services::CarrierService& carrier);

std::string handleGetFleet(services::FleetService& fleet);
std::string handleGetFleetIdle(services::FleetService& fleet);
std::string handleGetFleetUtilisation(services::FleetService& fleet);
std::string handlePostFleetRedeploy(
    const std::string& body,
    services::FleetService& fleet);

std::string handleGetColdChainAlerts(services::ColdChainService& cold);
std::string handleGetColdChainShipment(
    const std::string& shipment_id,
    services::ColdChainService& cold);
std::string handleGetColdChainTemperature(
    const std::string& shipment_id,
    services::ColdChainService& cold);

std::string handlePostSimulation(
    const std::string& body,
    services::SimulationService& sim);

std::string handleGetRecommendations(services::RecommendationService& rec_svc);

// Bob natural-language query handler
std::string handleBobQuery(
    const std::string& body,
    services::DisruptionService& dis,
    services::ShipmentService& ship,
    services::RouteService& route,
    services::CarrierService& carrier,
    services::FleetService& fleet,
    services::ColdChainService& cold,
    services::RecommendationService& rec_svc,
    db::Database& db);

// ── Driver / Request / Timeline endpoints ─────────────────────────────────────
std::string handleGetDrivers(db::Database& db);
std::string handleGetDriver(const std::string& driver_id, db::Database& db);
std::string handleGetDriverShipments(const std::string& driver_id, db::Database& db,
                                      services::ShipmentService& ship);

std::string handleGetDriverRequests(db::Database& db);
std::string handlePostDriverRequest(const std::string& body, db::Database& db);
std::string handleGetDriverRequest(const std::string& request_id, db::Database& db);
std::string handleApproveDriverRequest(const std::string& request_id,
                                        const std::string& body, db::Database& db);
std::string handleRejectDriverRequest(const std::string& request_id,
                                       const std::string& body, db::Database& db);

std::string handleGetNotifications(const std::string& driver_id, db::Database& db);
std::string handleMarkNotificationRead(const std::string& notif_id, db::Database& db);

std::string handleGetShipmentTimeline(const std::string& shipment_id, db::Database& db);
std::string handleGetShipmentLocation(const std::string& shipment_id, db::Database& db);
std::string handleGetShipmentRoute(const std::string& shipment_id, db::Database& db,
                                    services::ShipmentService& ship);

std::string handleGetDisruptionVerification(const std::string& disruption_event_id,
                                             db::Database& db);

std::string handlePostShipmentReroute(const std::string& shipment_id,
                                       const std::string& body, db::Database& db,
                                       services::ShipmentService& ship);

} // namespace api
