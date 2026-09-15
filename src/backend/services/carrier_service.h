#pragma once
#include "../models/carrier.h"
#include "../database/database.h"
#include <vector>
#include <string>

namespace services {

struct CarrierRecommendation {
    std::string shipment_id;
    std::string current_carrier;
    std::string current_carrier_name;
    std::string recommended_carrier;
    std::string recommended_carrier_name;
    std::string reason;
    std::string expected_impact;
    bool found;
};

class CarrierService {
public:
    explicit CarrierService(db::Database& db);

    std::vector<models::Carrier> getAllCarriers();
    models::Carrier getById(const std::string& id);

    // Recommend best available carrier for a shipment
    CarrierRecommendation recommend(const std::string& shipment_id,
                                    const std::string& current_carrier_id,
                                    int required_capacity_kg,
                                    bool needs_cold_chain,
                                    const std::string& service_region);

private:
    db::Database& db_;
    models::Carrier rowToCarrier(int col, const char** vals, const char** names);
};

} // namespace services
