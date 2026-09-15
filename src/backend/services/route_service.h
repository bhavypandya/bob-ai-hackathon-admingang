#pragma once
#include "../models/route.h"
#include "../database/database.h"
#include <vector>
#include <string>

namespace services {

struct RouteRecommendation {
    std::string shipment_id;
    std::string current_route;
    std::string recommended_route;
    std::string recommended_route_name;
    std::string original_eta;
    std::string new_eta;
    int additional_hours;
    double cost_usd;
    int risk_score;
    std::string reason;
    bool found;
};

class RouteService {
public:
    explicit RouteService(db::Database& db);

    std::vector<models::Route> getAllRoutes();
    models::Route getById(const std::string& id);

    // Find best available route for a shipment
    RouteRecommendation recommend(const std::string& shipment_id,
                                  const std::string& current_route_id,
                                  const std::string& origin,
                                  const std::string& destination,
                                  bool needs_refrigeration);

private:
    db::Database& db_;
    models::Route rowToRoute(int col, const char** vals, const char** names);
};

} // namespace services
