#include "recommendation_service.h"
#include <algorithm>
#include <sstream>

namespace services {

RecommendationService::RecommendationService(
    db::Database& db, DisruptionService& dis, ShipmentService& ship,
    RouteService& route, CarrierService& carrier, FleetService& fleet, ColdChainService& cold)
    : db_(db), dis_(dis), ship_(ship), route_(route), carrier_(carrier), fleet_(fleet), cold_(cold) {}

std::vector<models::Recommendation> RecommendationService::generateAll() {
    std::vector<models::Recommendation> recs;
    int id = 1;

    auto disruptions = dis_.getActiveDisruptions();
    auto affected = ship_.getAffectedShipments(disruptions);

    // 1. Reroute / carrier change for high-risk shipments
    for (const auto& s : affected) {
        if (s.impact_score < 25) continue;

        // Route recommendation
        bool needs_ref = s.cold_chain;
        auto route_rec = route_.recommend(s.id, s.current_route, s.origin, s.destination, needs_ref);
        if (route_rec.found && route_rec.recommended_route != s.current_route) {
            models::Recommendation r;
            r.id = id++;
            r.type = models::RecommendationType::REROUTE_SHIPMENT;
            r.priority = s.impact_score >= 75 ? models::RecommendationPriority::CRITICAL :
                         s.impact_score >= 50 ? models::RecommendationPriority::HIGH :
                                                models::RecommendationPriority::MEDIUM;
            r.target = s.id;
            r.reason = route_rec.reason;
            r.expected_impact = "Switch from route " + s.current_route + " to " +
                                 route_rec.recommended_route + " (" + route_rec.recommended_route_name + ").";
            r.action = "Reroute " + s.id + " via " + route_rec.recommended_route_name;
            recs.push_back(r);
        }

        // Carrier recommendation if carrier is disrupted
        models::Carrier cur_carrier;
        bool carrier_disrupted = false;
        db_.query("SELECT is_disrupted, available_capacity FROM carriers WHERE id='" + s.carrier + "'",
            [&](int, const char** v, const char**) {
                carrier_disrupted = v[0] && std::string(v[0]) == "1";
            });

        if (carrier_disrupted) {
            auto car_rec = carrier_.recommend(s.id, s.carrier, 5000, s.cold_chain, "");
            if (car_rec.found && car_rec.recommended_carrier != s.carrier) {
                models::Recommendation r;
                r.id = id++;
                r.type = models::RecommendationType::CHANGE_CARRIER;
                r.priority = s.impact_score >= 75 ? models::RecommendationPriority::CRITICAL
                                                   : models::RecommendationPriority::HIGH;
                r.target = s.id;
                r.reason = car_rec.reason;
                r.expected_impact = car_rec.expected_impact;
                r.action = "Change carrier for " + s.id + " from " + s.carrier + " to " + car_rec.recommended_carrier;
                recs.push_back(r);
            }
        }

        // Prioritise critical/cold-chain shipments
        if (s.priority == models::ShipmentPriority::CRITICAL || s.cold_chain) {
            models::Recommendation r;
            r.id = id++;
            r.type = models::RecommendationType::PRIORITISE_SHIPMENT;
            r.priority = models::RecommendationPriority::HIGH;
            r.target = s.id;
            r.reason = "Shipment has " + models::Shipment::priorityToString(s.priority) +
                       " priority" + (s.cold_chain ? " and is cold-chain sensitive" : "") + ".";
            r.expected_impact = "Ensure priority handling at next hub.";
            r.action = "Flag " + s.id + " for priority processing";
            recs.push_back(r);
        }
    }

    // 2. Fleet redeployment
    auto redeploy_recs = fleet_.getRedeploymentRecommendations();
    for (const auto& rr : redeploy_recs) {
        models::Recommendation r;
        r.id = id++;
        r.type = models::RecommendationType::REDEPLOY_FLEET;
        r.priority = models::RecommendationPriority::MEDIUM;
        r.target = rr.asset_id;
        r.reason = rr.reason;
        r.expected_impact = "Redeploy idle " + rr.asset_type + " from " +
                             rr.current_location + " to " + rr.target_location + ".";
        r.action = "Redeploy " + rr.asset_id + " to " + rr.target_location;
        recs.push_back(r);
    }

    // 3. Cold-chain reviews
    auto cold_alerts = cold_.getAllAlerts();
    for (const auto& alert : cold_alerts) {
        if (alert.severity == models::ExcursionSeverity::NORMAL) continue;

        models::Recommendation r;
        r.id = id++;
        r.type = models::RecommendationType::COLD_CHAIN_REVIEW;
        r.priority = alert.severity == models::ExcursionSeverity::CRITICAL
            ? models::RecommendationPriority::CRITICAL
            : alert.severity == models::ExcursionSeverity::HIGH
            ? models::RecommendationPriority::HIGH
            : models::RecommendationPriority::MEDIUM;
        r.target = alert.shipment_id;
        r.reason = alert.severity_reason;
        r.expected_impact = alert.recommended_action;
        r.action = "Cold-chain review: " + alert.shipment_id + " — configured severity: " +
                   models::ColdChainAlert::severityToString(alert.severity);
        recs.push_back(r);
    }

    // 4. Monitor disruptions
    for (const auto& d : disruptions) {
        models::Recommendation r;
        r.id = id++;
        r.type = models::RecommendationType::MONITOR_DISRUPTION;
        r.priority = d.severity == models::DisruptionSeverity::CRITICAL
            ? models::RecommendationPriority::HIGH
            : models::RecommendationPriority::MEDIUM;
        r.target = "Disruption-" + std::to_string(d.id);
        r.reason = d.name + " at " + d.location + " is " + models::Disruption::statusToString(d.status) + ".";
        r.expected_impact = "Monitor until resolved. Expected end: " + d.expected_end_time + ".";
        r.action = "Monitor " + d.name;
        recs.push_back(r);
    }

    // Sort by priority
    std::sort(recs.begin(), recs.end(), [](const models::Recommendation& a, const models::Recommendation& b) {
        return static_cast<int>(a.priority) > static_cast<int>(b.priority);
    });

    return recs;
}

std::string RecommendationService::generateBobSummary() {
    auto disruptions = dis_.getActiveDisruptions();
    auto affected = ship_.getAffectedShipments(disruptions);
    auto idle = fleet_.getIdleAssets();
    auto cold_alerts = cold_.getAllAlerts();
    auto util = fleet_.getUtilisation();

    std::ostringstream out;

    out << "Current situation:\n\n";

    // Disruptions
    out << disruptions.size() << " active disruption(s):\n";
    for (const auto& d : disruptions) {
        out << "  • " << d.name << " (" << models::Disruption::typeToString(d.type) << ") at "
            << d.location << " — " << models::Disruption::severityToString(d.severity) << "\n";
    }
    out << "\n";

    // Affected shipments
    out << affected.size() << " shipment(s) are affected.\n";
    if (!affected.empty()) {
        out << "\nHighest-risk shipment:\n";
        const auto& top = affected[0];
        out << "  " << top.id << " (Risk: " << top.risk_level << ", Score: " << top.impact_score << ")\n";
        out << "  Route: " << top.current_route << "  |  Carrier: " << top.carrier << "\n";
        out << "  Reasons:\n";
        for (const auto& reason : top.impact_reasons) {
            out << "    - " << reason << "\n";
        }

        // Route recommendation for top shipment
        auto rr = route_.recommend(top.id, top.current_route, top.origin, top.destination, top.cold_chain);
        if (rr.found && rr.recommended_route != top.current_route) {
            out << "\n  Recommended route: " << rr.recommended_route
                << " (" << rr.recommended_route_name << ")\n";
            out << "  Reason: " << rr.reason << "\n";
        }
    }
    out << "\n";

    // Fleet
    out << "Fleet utilisation:\n";
    out << "  Total assets: " << util.total_assets
        << "  |  Active/in-transit: " << util.active_assets
        << "  |  Idle: " << util.idle_assets << "\n";
    out << "  Average utilisation: " << static_cast<int>(util.average_utilisation_pct) << "%\n";
    if (!idle.empty()) {
        out << "\n  Idle assets available for redeployment:\n";
        for (const auto& a : idle) {
            out << "    • " << a.id << " (" << models::FleetAsset::typeToString(a.type) << ") at " << a.location << "\n";
        }
    }
    out << "\n";

    // Cold chain
    int cold_issues = 0;
    for (const auto& alert : cold_alerts) {
        if (alert.severity != models::ExcursionSeverity::NORMAL) cold_issues++;
    }
    out << "Cold-chain status: " << cold_issues << " shipment(s) require attention.\n";
    for (const auto& alert : cold_alerts) {
        if (alert.severity == models::ExcursionSeverity::NORMAL) continue;
        out << "  • " << alert.shipment_id
            << " — Configured severity: " << models::ColdChainAlert::severityToString(alert.severity) << "\n";
        out << "    " << alert.severity_reason << "\n";
        out << "    Action: " << alert.recommended_action << "\n";
    }
    out << "\n";

    // Top actions
    out << "Recommended next actions:\n";
    auto recs = generateAll();
    int shown = 0;
    for (const auto& r : recs) {
        if (shown >= 5) break;
        if (r.priority >= models::RecommendationPriority::HIGH) {
            out << "  " << (shown + 1) << ". " << r.action << "\n";
            shown++;
        }
    }
    if (shown == 0) {
        out << "  No critical actions required at this time.\n";
    }

    out << "\nNote: Cold-chain severity is based on configured thresholds. "
        << "Review against applicable product handling requirements.";

    return out.str();
}

} // namespace services
