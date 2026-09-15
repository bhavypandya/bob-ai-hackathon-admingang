#pragma once
#include <string>

namespace models {

struct Carrier {
    std::string id;
    std::string name;
    std::string type;               // ROAD / RAIL / SEA / AIR / MULTI
    int available_capacity_kg;
    int total_capacity_kg;
    double reliability_score;       // 0-1
    double cost_per_km;
    int avg_delay_hours;
    bool cold_chain_capable;
    bool is_disrupted;
    std::string disruption_reason;
    std::string service_region;
};

} // namespace models
