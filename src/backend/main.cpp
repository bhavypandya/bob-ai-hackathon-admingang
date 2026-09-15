/*
 * Supply Chain Disruption Assistant & Fleet Utilisation Optimizer
 * Backend server — C++17 with built-in TCP HTTP server
 * No external HTTP framework required (Crow/Boost optional via -DUSE_CROW)
 */

#include "database/database.h"
#include "api/api_handler.h"
#include "services/disruption_service.h"
#include "services/shipment_service.h"
#include "services/route_service.h"
#include "services/carrier_service.h"
#include "services/fleet_service.h"
#include "services/cold_chain_service.h"
#include "services/recommendation_service.h"
#include "services/simulation_service.h"

#include <iostream>
#include <string>
#include <sstream>
#include <cstring>
#include <functional>
#include <map>
#include <vector>
#include <algorithm>

// Platform-specific socket headers
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET socket_t;
  #define CLOSE_SOCKET closesocket
  #define INVALID_SOCK INVALID_SOCKET
  #define SOCK_ERR SOCKET_ERROR
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  typedef int socket_t;
  #define CLOSE_SOCKET close
  #define INVALID_SOCK -1
  #define SOCK_ERR -1
#endif

// ── Minimal HTTP parser ────────────────────────────────────────────────────────
struct HttpRequest {
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
};

struct HttpResponse {
    int status = 200;
    std::string content_type = "application/json";
    std::string body;
};

static bool parseRequest(const std::string& raw, HttpRequest& req) {
    size_t header_end = raw.find("\r\n\r\n");
    if (header_end == std::string::npos) return false;

    std::string header_section = raw.substr(0, header_end);
    req.body = raw.substr(header_end + 4);

    std::istringstream ss(header_section);
    std::string line;
    std::getline(ss, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream first(line);
    first >> req.method >> req.path;

    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string val = line.substr(colon + 1);
            while (!val.empty() && val[0] == ' ') val = val.substr(1);
            std::transform(key.begin(), key.end(), key.begin(), ::tolower);
            req.headers[key] = val;
        }
    }

    // Trim path to remove query string
    size_t q = req.path.find('?');
    if (q != std::string::npos) req.path = req.path.substr(0, q);

    return true;
}

static std::string buildResponse(const HttpResponse& resp) {
    std::string status_text = "OK";
    if (resp.status == 400) status_text = "Bad Request";
    else if (resp.status == 404) status_text = "Not Found";
    else if (resp.status == 500) status_text = "Internal Server Error";

    std::string r;
    r += "HTTP/1.1 " + std::to_string(resp.status) + " " + status_text + "\r\n";
    r += "Content-Type: " + resp.content_type + "; charset=utf-8\r\n";
    r += "Content-Length: " + std::to_string(resp.body.size()) + "\r\n";
    r += "Access-Control-Allow-Origin: *\r\n";
    r += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    r += "Access-Control-Allow-Headers: Content-Type\r\n";
    r += "Connection: close\r\n";
    r += "\r\n";
    r += resp.body;
    return r;
}

// ── Router ────────────────────────────────────────────────────────────────────
using Handler = std::function<HttpResponse(const HttpRequest&)>;

struct Route {
    std::string method;
    std::string pattern;      // e.g. "/api/cold-chain/{id}/temperature"
    Handler handler;
};

static std::vector<Route> g_routes;

static void addRoute(const std::string& method, const std::string& pattern, Handler h) {
    g_routes.push_back({method, pattern, h});
}

// Extract path parameter — returns true if path matches pattern
static bool matchPath(const std::string& pattern, const std::string& path,
                       std::map<std::string, std::string>& params) {
    std::vector<std::string> pat_parts, path_parts;
    auto split = [](const std::string& s, char delim, std::vector<std::string>& out) {
        std::istringstream ss(s);
        std::string tok;
        while (std::getline(ss, tok, delim))
            if (!tok.empty()) out.push_back(tok);
    };
    split(pattern, '/', pat_parts);
    split(path, '/', path_parts);
    if (pat_parts.size() != path_parts.size()) return false;
    for (size_t i = 0; i < pat_parts.size(); ++i) {
        if (pat_parts[i].size() > 2 && pat_parts[i].front() == '{' && pat_parts[i].back() == '}') {
            std::string key = pat_parts[i].substr(1, pat_parts[i].size() - 2);
            params[key] = path_parts[i];
        } else if (pat_parts[i] != path_parts[i]) {
            return false;
        }
    }
    return true;
}

static HttpResponse dispatch(const HttpRequest& req) {
    if (req.method == "OPTIONS") {
        HttpResponse r;
        r.status = 200;
        r.body = "";
        return r;
    }
    for (const auto& route : g_routes) {
        if (route.method != req.method) continue;
        std::map<std::string, std::string> params;
        if (matchPath(route.pattern, req.path, params)) {
            HttpRequest req2 = req;
            req2.headers["_param_shipment_id"] = params.count("shipment_id") ? params.at("shipment_id") : "";
            req2.headers["_param_id"] = params.count("id") ? params.at("id") : "";
            return route.handler(req2);
        }
    }
    // Static file fallback
    HttpResponse r;
    r.status = 404;
    r.body = "{\"error\":\"Not found\"}";
    return r;
}

// ── Connection handler ─────────────────────────────────────────────────────────
static void handleConnection(socket_t client_sock) {
    // Set a 5-second receive timeout so the server never blocks indefinitely
#ifdef _WIN32
    DWORD tv = 5000;
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#else
    struct timeval tv{5, 0};
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    char buf[131072] = {};
    int total = 0;
    int n;

    // Read until we have complete headers
    while (total < (int)sizeof(buf) - 1) {
        n = recv(client_sock, buf + total, sizeof(buf) - total - 1, 0);
        if (n <= 0) break;
        total += n;
        if (strstr(buf, "\r\n\r\n") != nullptr) break;
    }

    if (total > 0) {
        // Find end of headers
        const char* hdr_end = strstr(buf, "\r\n\r\n");
        if (hdr_end) {
            int headers_len = (int)(hdr_end - buf) + 4;

            // Parse Content-Length if present
            const char* cl_pos = strstr(buf, "Content-Length:");
            if (!cl_pos) cl_pos = strstr(buf, "content-length:");
            int content_length = 0;
            if (cl_pos) {
                content_length = atoi(cl_pos + 15);
            }

            // Read remaining body bytes
            int body_have = total - headers_len;
            while (body_have < content_length && total < (int)sizeof(buf) - 1) {
                n = recv(client_sock, buf + total, content_length - body_have, 0);
                if (n <= 0) break;
                total += n;
                body_have += n;
            }
        }

        HttpRequest req;
        if (parseRequest(std::string(buf, total), req)) {
            try {
                HttpResponse resp = dispatch(req);
                std::string raw = buildResponse(resp);
                send(client_sock, raw.c_str(), (int)raw.size(), 0);
            } catch (const std::exception& e) {
                std::string err = "{\"error\":\"" + std::string(e.what()) + "\"}";
                HttpResponse r; r.status = 500; r.body = err;
                std::string raw = buildResponse(r);
                send(client_sock, raw.c_str(), (int)raw.size(), 0);
            }
        }
    }
    CLOSE_SOCKET(client_sock);
}

// ── Service file serving ───────────────────────────────────────────────────────
#include <fstream>
#include <cstdint>
static HttpResponse serveFile(const std::string& filepath, const std::string& content_type) {
    std::ifstream f(filepath, std::ios::binary);
    HttpResponse r;
    if (!f.is_open()) {
        r.status = 404;
        r.body = "File not found";
        r.content_type = "text/plain";
        return r;
    }
    r.content_type = content_type;
    r.body = std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return r;
}

// ── Main ───────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    int port = 8080;
    std::string db_path = ":memory:";
    std::string frontend_dir = "./frontend";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) port = std::stoi(argv[++i]);
        else if (arg == "--db" && i + 1 < argc) db_path = argv[++i];
        else if (arg == "--frontend" && i + 1 < argc) frontend_dir = argv[++i];
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    std::cout << "=== Supply Chain Disruption Assistant ===\n";
    std::cout << "Initialising database...\n";

    db::initDB(db_path);
    db::Database& database = db::getDB();

    // Instantiate services
    services::DisruptionService dis_svc(database);
    services::ShipmentService ship_svc(database);
    services::RouteService route_svc(database);
    services::CarrierService carrier_svc(database);
    services::FleetService fleet_svc(database);
    services::ColdChainService cold_svc(database);
    services::RecommendationService rec_svc(database, dis_svc, ship_svc, route_svc,
                                             carrier_svc, fleet_svc, cold_svc);
    services::SimulationService sim_svc(database, dis_svc, ship_svc, fleet_svc, cold_svc);

    std::cout << "Registering API routes...\n";

    // ── Register API routes ─────────────────────────────────────────────────
    addRoute("GET", "/api/health", [](const HttpRequest&) {
        return HttpResponse{200, "application/json", api::handleHealth()};
    });

    addRoute("GET", "/api/dashboard", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json",
            api::handleDashboard(database, dis_svc, ship_svc, fleet_svc, cold_svc, rec_svc)};
    });

    addRoute("GET", "/api/shipments", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json", api::handleGetShipments(ship_svc)};
    });

    addRoute("GET", "/api/shipments/affected", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json",
            api::handleGetAffectedShipments(ship_svc, dis_svc)};
    });

    addRoute("GET", "/api/disruptions", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json", api::handleGetDisruptions(dis_svc)};
    });

    addRoute("GET", "/api/routes", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json", api::handleGetRoutes(route_svc)};
    });

    addRoute("POST", "/api/routes/recommend", [&](const HttpRequest& req) {
        return HttpResponse{200, "application/json",
            api::handlePostRouteRecommend(req.body, ship_svc, route_svc)};
    });

    addRoute("GET", "/api/carriers", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json", api::handleGetCarriers(carrier_svc)};
    });

    addRoute("POST", "/api/carriers/recommend", [&](const HttpRequest& req) {
        return HttpResponse{200, "application/json",
            api::handlePostCarrierRecommend(req.body, ship_svc, carrier_svc)};
    });

    addRoute("GET", "/api/fleet", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json", api::handleGetFleet(fleet_svc)};
    });

    addRoute("GET", "/api/fleet/idle", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json", api::handleGetFleetIdle(fleet_svc)};
    });

    addRoute("GET", "/api/fleet/utilisation", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json", api::handleGetFleetUtilisation(fleet_svc)};
    });

    addRoute("POST", "/api/fleet/redeploy", [&](const HttpRequest& req) {
        return HttpResponse{200, "application/json",
            api::handlePostFleetRedeploy(req.body, fleet_svc)};
    });

    addRoute("GET", "/api/cold-chain/alerts", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json", api::handleGetColdChainAlerts(cold_svc)};
    });

    addRoute("GET", "/api/cold-chain/{shipment_id}", [&](const HttpRequest& req) {
        std::string sid = req.headers.count("_param_shipment_id") ?
            req.headers.at("_param_shipment_id") : "";
        return HttpResponse{200, "application/json",
            api::handleGetColdChainShipment(sid, cold_svc)};
    });

    addRoute("GET", "/api/cold-chain/{shipment_id}/temperature", [&](const HttpRequest& req) {
        std::string sid = req.headers.count("_param_shipment_id") ?
            req.headers.at("_param_shipment_id") : "";
        return HttpResponse{200, "application/json",
            api::handleGetColdChainTemperature(sid, cold_svc)};
    });

    addRoute("POST", "/api/simulation", [&](const HttpRequest& req) {
        return HttpResponse{200, "application/json",
            api::handlePostSimulation(req.body, sim_svc)};
    });

    addRoute("GET", "/api/recommendations", [&](const HttpRequest&) {
        return HttpResponse{200, "application/json",
            api::handleGetRecommendations(rec_svc)};
    });

    addRoute("POST", "/api/bob/query", [&](const HttpRequest& req) {
        return HttpResponse{200, "application/json",
            api::handleBobQuery(req.body, dis_svc, ship_svc, route_svc,
                                carrier_svc, fleet_svc, cold_svc, rec_svc)};
    });

    // ── Static file serving ─────────────────────────────────────────────────
    addRoute("GET", "/", [&](const HttpRequest&) {
        return serveFile(frontend_dir + "/index.html", "text/html");
    });
    addRoute("GET", "/index.html", [&](const HttpRequest&) {
        return serveFile(frontend_dir + "/index.html", "text/html");
    });
    addRoute("GET", "/css/dashboard.css", [&](const HttpRequest&) {
        return serveFile(frontend_dir + "/css/dashboard.css", "text/css");
    });
    addRoute("GET", "/js/dashboard.js", [&](const HttpRequest&) {
        return serveFile(frontend_dir + "/js/dashboard.js", "application/javascript");
    });

    // ── Start TCP server ────────────────────────────────────────────────────
    socket_t server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == INVALID_SOCK) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(server_sock, (sockaddr*)&addr, sizeof(addr)) == SOCK_ERR) {
        std::cerr << "Failed to bind port " << port << "\n";
        CLOSE_SOCKET(server_sock);
        return 1;
    }

    if (listen(server_sock, 64) == SOCK_ERR) {
        std::cerr << "Failed to listen\n";
        CLOSE_SOCKET(server_sock);
        return 1;
    }

    std::cout << "\n✓ Server running on http://localhost:" << port << "\n";
    std::cout << "✓ Dashboard: http://localhost:" << port << "/\n";
    std::cout << "✓ API health: http://localhost:" << port << "/api/health\n";
    std::cout << "\nPress Ctrl+C to stop.\n\n";

    while (true) {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int client_len = sizeof(client_addr);
#else
        socklen_t client_len = sizeof(client_addr);
#endif
        socket_t client_sock = accept(server_sock, (sockaddr*)&client_addr, &client_len);
        if (client_sock == INVALID_SOCK) continue;

        // Handle each connection sequentially
        handleConnection(client_sock);
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
