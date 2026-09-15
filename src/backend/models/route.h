#pragma once
#include <string>
#include <vector>

namespace models {

struct RouteSegment {
    std::string from;
    std::string to;
    std::string mode;   // ROAD / RAIL / SEA / AIR
    double distance_km;
    double transit_hours;
};

struct Route {
    std::string id;
    std::string name;
    std::string origin;
    std::string destination;
    std::vector<RouteSegment> segments;
    double total_distance_km;
    double estimated_hours;
    double cost_usd;
    int capacity_kg;
    bool is_disrupted;
    std::string disruption_reason;
    int risk_score;             // 0-100
    bool refrigerated_capable;
};

} // namespace models
