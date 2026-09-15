#pragma once
#include "../models/disruption.h"
#include "../models/shipment.h"
#include "../database/database.h"
#include <vector>
#include <string>

namespace services {

class DisruptionService {
public:
    explicit DisruptionService(db::Database& db);

    std::vector<models::Disruption> getAllDisruptions();
    std::vector<models::Disruption> getActiveDisruptions();
    models::Disruption getById(int id);

    // Returns shipments affected by a given disruption
    std::vector<std::string> getAffectedShipmentIds(const models::Disruption& d);

    // Mark a disruption as active (used by simulation)
    void setStatus(int disruption_id, const std::string& status);

    // Add a new disruption (simulation)
    int addDisruption(const models::Disruption& d);

    // Mark a route as disrupted
    void setRouteDisrupted(const std::string& route_id, bool disrupted, const std::string& reason);

    // Mark a carrier as disrupted
    void setCarrierDisrupted(const std::string& carrier_id, bool disrupted, const std::string& reason);

private:
    db::Database& db_;
    models::Disruption rowToDisruption(int col, const char** vals, const char** names);
};

} // namespace services
