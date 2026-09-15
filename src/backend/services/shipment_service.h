#pragma once
#include "../models/shipment.h"
#include "../models/disruption.h"
#include "../database/database.h"
#include <vector>
#include <string>

namespace services {

class ShipmentService {
public:
    explicit ShipmentService(db::Database& db);

    std::vector<models::Shipment> getAllShipments();
    models::Shipment getById(const std::string& id);

    // Analyse which shipments are affected by active disruptions and compute risk scores
    std::vector<models::Shipment> getAffectedShipments(const std::vector<models::Disruption>& active_disruptions);

    // Update a shipment's status/ETA after a simulation event
    void updateStatus(const std::string& id, const std::string& status, int delay_hours);

private:
    db::Database& db_;
    models::Shipment rowToShipment(int col, const char** vals, const char** names);

    // Compute an impact score for a shipment given active disruptions
    int computeImpactScore(const models::Shipment& s,
                           const std::vector<models::Disruption>& disruptions,
                           std::vector<std::string>& reasons);
};

} // namespace services
