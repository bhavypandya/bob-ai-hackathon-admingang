#pragma once
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

// Forward-declare sqlite3 to avoid including sqlite3.h everywhere
struct sqlite3;
struct sqlite3_stmt;

namespace db {

// Simple RAII wrapper around SQLite
class Database {
public:
    explicit Database(const std::string& path = ":memory:");
    ~Database();

    // No copy
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // Execute a statement with no result rows
    void exec(const std::string& sql);

    // Query with a row callback: callback(column_count, values[], column_names[])
    void query(const std::string& sql,
               std::function<void(int, const char**, const char**)> row_cb);

    // Seed all demo data into the DB
    void seedData();

    // Reset to fresh demo state (used by simulation)
    void resetToSeedState();

    sqlite3* handle() { return db_; }

private:
    sqlite3* db_ = nullptr;
    std::string path_;

    void createSchema();
    void seedDisruptions();
    void seedRoutes();
    void seedCarriers();
    void seedShipments();
    void seedFleetAssets();
    void seedSensorReadings();
    void seedColdChainConfig();
    // New seed methods for driver/request/timeline features
    void seedDrivers();
    void seedDriverRequests();
    void seedNotifications();
    void seedShipmentCheckpoints();
    void seedShipmentLocations();
    void seedDisruptionEvents();
    void seedReroutes();
    void seedVerificationSources();
};

// Global singleton accessor
Database& getDB();
void initDB(const std::string& path = ":memory:");

} // namespace db
