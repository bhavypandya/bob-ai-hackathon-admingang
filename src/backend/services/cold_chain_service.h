#pragma once
#include "../models/cold_chain.h"
#include "../database/database.h"
#include <vector>
#include <string>

namespace services {

class ColdChainService {
public:
    explicit ColdChainService(db::Database& db);

    // Get all cold-chain shipment IDs
    std::vector<std::string> getColdChainShipmentIds();

    // Get full alert for a shipment
    models::ColdChainAlert getAlert(const std::string& shipment_id);

    // Get all alerts (returns all cold-chain shipments with computed severity)
    std::vector<models::ColdChainAlert> getAllAlerts();

    // Get sensor readings for a shipment
    std::vector<models::SensorReading> getReadings(const std::string& shipment_id);

    // Get config
    models::ColdChainConfig getConfig(const std::string& shipment_id);

    // Add a simulated sensor reading (used by simulation)
    void addReading(const std::string& shipment_id, double temperature, double humidity);

private:
    db::Database& db_;

    // Classify the excursion given config and readings
    models::ExcursionSeverity classify(const models::ColdChainConfig& cfg,
                                        double peak_temp,
                                        int excursion_minutes,
                                        int excursion_count,
                                        int minutes_to_delivery);

    std::string buildRecommendedAction(models::ExcursionSeverity sev,
                                        const std::string& shipment_id);
};

} // namespace services
