#include "database.h"
#include <sqlite3.h>
#include <stdexcept>
#include <iostream>

namespace db {

// ── Global instance ───────────────────────────────────────────────────────────
static Database* g_db = nullptr;

Database& getDB() {
    if (!g_db) throw std::runtime_error("Database not initialised — call initDB() first");
    return *g_db;
}

void initDB(const std::string& path) {
    static Database instance(path);
    g_db = &instance;
}

// ── Constructor / Destructor ──────────────────────────────────────────────────
Database::Database(const std::string& path) : path_(path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("Cannot open SQLite database: " + err);
    }
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA foreign_keys=ON");
    createSchema();
    seedData();
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

// ── exec / query ─────────────────────────────────────────────────────────────
void Database::exec(const std::string& sql) {
    char* errmsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::string err(errmsg);
        sqlite3_free(errmsg);
        throw std::runtime_error("SQLite exec error: " + err + "\nSQL: " + sql);
    }
}

void Database::query(const std::string& sql,
                     std::function<void(int, const char**, const char**)> row_cb) {
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("SQLite prepare error: " + std::string(sqlite3_errmsg(db_)));
    }
    int col_count = sqlite3_column_count(stmt);
    // Build column name array once
    std::vector<const char*> col_names(col_count);
    for (int i = 0; i < col_count; ++i)
        col_names[i] = sqlite3_column_name(stmt, i);

    while (true) {
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            std::vector<const char*> values(col_count);
            for (int i = 0; i < col_count; ++i)
                values[i] = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row_cb(col_count, values.data(), col_names.data());
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            sqlite3_finalize(stmt);
            throw std::runtime_error("SQLite step error: " + std::string(sqlite3_errmsg(db_)));
        }
    }
    sqlite3_finalize(stmt);
}

// ── Schema ────────────────────────────────────────────────────────────────────
void Database::createSchema() {
    exec(R"(
        CREATE TABLE IF NOT EXISTS disruptions (
            id          INTEGER PRIMARY KEY,
            type        TEXT NOT NULL,
            name        TEXT NOT NULL,
            location    TEXT NOT NULL,
            severity    TEXT NOT NULL,
            start_time  TEXT NOT NULL,
            end_time    TEXT NOT NULL,
            affected_routes TEXT NOT NULL,  -- comma-separated route IDs
            description TEXT NOT NULL,
            status      TEXT NOT NULL DEFAULT 'ACTIVE'
        );

        CREATE TABLE IF NOT EXISTS routes (
            id              TEXT PRIMARY KEY,
            name            TEXT NOT NULL,
            origin          TEXT NOT NULL,
            destination     TEXT NOT NULL,
            estimated_hours REAL NOT NULL,
            distance_km     REAL NOT NULL,
            cost_usd        REAL NOT NULL,
            capacity_kg     INTEGER NOT NULL,
            is_disrupted    INTEGER NOT NULL DEFAULT 0,
            disruption_reason TEXT NOT NULL DEFAULT '',
            risk_score      INTEGER NOT NULL DEFAULT 0,
            refrigerated    INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS carriers (
            id                  TEXT PRIMARY KEY,
            name                TEXT NOT NULL,
            type                TEXT NOT NULL,
            available_capacity  INTEGER NOT NULL,
            total_capacity      INTEGER NOT NULL,
            reliability         REAL NOT NULL,
            cost_per_km         REAL NOT NULL,
            avg_delay_hours     INTEGER NOT NULL DEFAULT 0,
            cold_chain          INTEGER NOT NULL DEFAULT 0,
            is_disrupted        INTEGER NOT NULL DEFAULT 0,
            disruption_reason   TEXT NOT NULL DEFAULT '',
            service_region      TEXT NOT NULL DEFAULT ''
        );

        CREATE TABLE IF NOT EXISTS shipments (
            id                  TEXT PRIMARY KEY,
            origin              TEXT NOT NULL,
            destination         TEXT NOT NULL,
            current_route       TEXT NOT NULL,
            carrier             TEXT NOT NULL,
            fleet_asset         TEXT NOT NULL DEFAULT '',
            status              TEXT NOT NULL,
            priority            TEXT NOT NULL,
            cargo_type          TEXT NOT NULL,
            cold_chain          INTEGER NOT NULL DEFAULT 0,
            planned_departure   TEXT NOT NULL,
            planned_arrival     TEXT NOT NULL,
            current_eta         TEXT NOT NULL,
            expected_delay_hours INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS fleet_assets (
            id                  TEXT PRIMARY KEY,
            type                TEXT NOT NULL,
            location            TEXT NOT NULL,
            status              TEXT NOT NULL,
            capacity_kg         INTEGER NOT NULL,
            current_assignment  TEXT NOT NULL DEFAULT '',
            availability_time   TEXT NOT NULL DEFAULT '',
            utilisation_pct     INTEGER NOT NULL DEFAULT 0,
            refrigerated        INTEGER NOT NULL DEFAULT 0
        );

        CREATE TABLE IF NOT EXISTS sensor_readings (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            shipment_id TEXT NOT NULL,
            timestamp   TEXT NOT NULL,
            temperature REAL NOT NULL,
            humidity    REAL NOT NULL,
            sensor_status TEXT NOT NULL DEFAULT 'OK'
        );

        CREATE TABLE IF NOT EXISTS cold_chain_config (
            shipment_id     TEXT PRIMARY KEY,
            min_temp        REAL NOT NULL,
            max_temp        REAL NOT NULL,
            warning_margin  REAL NOT NULL DEFAULT 1.0,
            critical_margin REAL NOT NULL DEFAULT 3.0,
            allowed_excursion_minutes INTEGER NOT NULL DEFAULT 30
        );
    )");
}

// ── Seed Data ─────────────────────────────────────────────────────────────────
void Database::seedData() {
    seedDisruptions();
    seedRoutes();
    seedCarriers();
    seedShipments();
    seedFleetAssets();
    seedColdChainConfig();
    seedSensorReadings();
}

void Database::seedDisruptions() {
    exec("DELETE FROM disruptions");
    exec(R"(
        INSERT INTO disruptions VALUES
        (1,'Weather Event','Cyclone Biparjoy','Arabian Sea / Mumbai Port',
         'CRITICAL','2024-06-10 08:00','2024-06-12 20:00',
         'R01,R02,R08','Severe cyclonic storm impacting Mumbai port operations and coastal road NH-48. All vessels delayed 36-48 hours. Road freight severely disrupted.','ACTIVE'),
        (2,'Port Strike','JNPT Labour Strike','Nhava Sheva / JNPT',
         'HIGH','2024-06-11 06:00','2024-06-13 18:00',
         'R03,R09','Dockworkers union strike at Jawaharlal Nehru Port. Container handling suspended. Estimated 48-hour delay on all sea-freight through JNPT.','ACTIVE'),
        (3,'Road Closure','NH-48 Bridge Repair','Pune–Mumbai Highway',
         'MEDIUM','2024-06-10 00:00','2024-06-11 23:59',
         'R01,R04','Emergency structural repair on NH-48 overpass near Khopoli. Single-lane traffic only. Estimated 3-4 hour delay for road freight.','ACTIVE'),
        (4,'Carrier Disruption','BlueLine Logistics Fleet Recall','Pan India',
         'HIGH','2024-06-10 12:00','2024-06-12 12:00',
         'R05,R06','BlueLine Logistics issued a fleet recall for brake inspection. Approximately 40% of their truck fleet offline.','MONITORING'),
        (5,'Geopolitical Crisis','Hormuz Strait Shipping Advisory','Persian Gulf',
         'MEDIUM','2024-06-09 00:00','2024-06-20 00:00',
         'R07','International shipping advisory for Hormuz Strait. Rerouting via Cape of Good Hope adding 8-10 days to Middle East routes.','MONITORING')
    )");
}

void Database::seedRoutes() {
    exec("DELETE FROM routes");
    exec(R"(
        INSERT INTO routes VALUES
        ('R01','Mumbai–Delhi Highway (NH-48)','Mumbai','Delhi',18.0,1400,850,25000,1,'NH-48 partially closed and cyclone impact',45,0),
        ('R02','Mumbai Port Sea Route','Mumbai','Chennai',52.0,1200,600,80000,1,'Cyclone Biparjoy — port closed',70,1),
        ('R03','JNPT Sea Freight to Kolkata','JNPT','Kolkata',96.0,2100,900,120000,1,'JNPT labour strike — no container handling',80,1),
        ('R04','Pune–Mumbai Road (Alt via Expressway)','Pune','Mumbai',3.5,150,120,20000,0,'',10,0),
        ('R05','Mumbai–Ahmedabad Highway','Mumbai','Ahmedabad',8.0,530,320,22000,0,'',12,0),
        ('R06','Delhi–Jaipur Highway (NH-48)','Delhi','Jaipur',5.5,280,180,18000,0,'',8,0),
        ('R07','Sea Route via Gulf','Mumbai','Dubai',120.0,2400,1800,150000,1,'Hormuz advisory — reroute via Cape',35,1),
        ('R08','Coastal Road NH-66 Mumbai–Goa','Mumbai','Goa',12.0,595,380,15000,1,'Cyclone Biparjoy — coastal roads damaged',55,0),
        ('R09','Chennai Sea Freight to JNPT','Chennai','JNPT',60.0,1350,750,90000,1,'JNPT strike — containers queued',60,1),
        ('R10','Delhi–Kolkata NH-19 Highway','Delhi','Kolkata',22.0,1504,920,25000,0,'',15,0),
        ('R11','Mumbai–Delhi Rail Freight','Mumbai','Delhi',24.0,1442,680,40000,0,'',8,1),
        ('R12','Pune–Delhi Highway via Nashik','Pune','Delhi',20.0,1524,930,23000,0,'',18,0),
        ('R13','Ahmedabad–Mumbai Alt Road','Ahmedabad','Mumbai',9.0,545,340,20000,0,'',10,0),
        ('R14','Chennai–Bangalore NH-48','Chennai','Bangalore',6.0,346,220,18000,0,'',9,0),
        ('R15','Mumbai–Chennai Rail Express','Mumbai','Chennai',22.0,1338,700,42000,0,'',7,1)
    )");
}

void Database::seedCarriers() {
    exec("DELETE FROM carriers");
    exec(R"(
        INSERT INTO carriers VALUES
        ('C01','SwiftMove Logistics','ROAD',85000,100000,0.92,4.5,1,0,0,'','West India'),
        ('C02','BlueLine Logistics','ROAD',30000,80000,0.78,3.8,3,0,1,'Fleet recall in progress','Pan India'),
        ('C03','OceanWave Shipping','SEA',200000,300000,0.88,1.2,4,1,1,'JNPT strike impact','Pan India'),
        ('C04','AirCargo Express','AIR',8000,12000,0.97,25.0,0,1,0,'','Pan India'),
        ('C05','RailFreight India','RAIL',180000,220000,0.91,2.1,2,1,0,'','Pan India'),
        ('C06','CoolChain Carriers','ROAD',40000,50000,0.94,6.2,1,1,0,'','South India'),
        ('C07','PrimeFreight Ltd','ROAD',70000,90000,0.89,5.1,2,0,0,'','North India'),
        ('C08','FastTrack Couriers','ROAD',25000,30000,0.85,7.0,1,0,0,'','Pan India'),
        ('C09','IndoShip Lines','SEA',280000,350000,0.86,1.0,6,1,1,'','East India'),
        ('C10','MountainPass Freight','ROAD',35000,45000,0.82,4.0,3,0,0,'','Central India')
    )");
}

void Database::seedShipments() {
    exec("DELETE FROM shipments");
    exec(R"(
        INSERT INTO shipments VALUES
        ('SH1001','Mumbai','Delhi','R01','C01','T201','DISRUPTED','CRITICAL','Pharmaceuticals',1,'2024-06-10 06:00','2024-06-11 00:00','2024-06-12 06:00',30),
        ('SH1002','JNPT','Kolkata','R03','C03','V301','DISRUPTED','HIGH','Electronics',0,'2024-06-10 08:00','2024-06-14 00:00','2024-06-16 12:00',60),
        ('SH1003','Mumbai','Chennai','R02','C03','V302','DISRUPTED','HIGH','Automotive Parts',0,'2024-06-10 10:00','2024-06-12 14:00','2024-06-14 20:00',54),
        ('SH1004','Pune','Mumbai','R04','C01','T202','ON_TIME','MEDIUM','Consumer Goods',0,'2024-06-10 07:00','2024-06-10 10:30','2024-06-10 10:30',0),
        ('SH1005','Mumbai','Ahmedabad','R05','C02','T203','DELAYED','HIGH','Industrial Equipment',0,'2024-06-10 09:00','2024-06-10 17:00','2024-06-10 20:00',3),
        ('SH1006','Mumbai','Goa','R08','C01','T204','DISRUPTED','MEDIUM','Food Products',0,'2024-06-10 11:00','2024-06-10 23:00','2024-06-11 14:00',15),
        ('SH1007','Delhi','Jaipur','R06','C07','T205','ON_TIME','LOW','Textiles',0,'2024-06-10 08:00','2024-06-10 13:30','2024-06-10 13:30',0),
        ('SH1008','Mumbai','Dubai','R07','C03','V303','DELAYED','HIGH','Petroleum Products',0,'2024-06-09 00:00','2024-06-14 00:00','2024-06-22 00:00',192),
        ('SH1009','Chennai','JNPT','R09','C03','V304','DISRUPTED','CRITICAL','Medical Devices',1,'2024-06-10 06:00','2024-06-12 18:00','2024-06-14 12:00',42),
        ('SH1010','Delhi','Kolkata','R10','C07','T206','ON_TIME','MEDIUM','Machine Parts',0,'2024-06-10 07:00','2024-06-11 05:00','2024-06-11 05:00',0),
        ('SH1011','Mumbai','Delhi','R11','C05','CN401','ON_TIME','HIGH','Chemicals',1,'2024-06-10 06:00','2024-06-11 06:00','2024-06-11 06:00',0),
        ('SH1012','Pune','Delhi','R12','C07','T207','AT_RISK','MEDIUM','Spare Parts',0,'2024-06-10 08:00','2024-06-11 04:00','2024-06-11 08:00',4),
        ('SH1013','Ahmedabad','Mumbai','R13','C01','T208','ON_TIME','LOW','Raw Materials',0,'2024-06-10 06:00','2024-06-10 15:00','2024-06-10 15:00',0),
        ('SH1014','Chennai','Bangalore','R14','C06','T209','ON_TIME','MEDIUM','Frozen Foods',1,'2024-06-10 07:00','2024-06-10 13:00','2024-06-10 13:00',0),
        ('SH1015','Mumbai','Chennai','R15','C05','CN402','ON_TIME','HIGH','IT Equipment',0,'2024-06-10 05:00','2024-06-11 03:00','2024-06-11 03:00',0),
        ('SH1016','Mumbai','Delhi','R01','C02','T210','DISRUPTED','CRITICAL','Vaccines',1,'2024-06-10 05:00','2024-06-10 23:00','2024-06-12 14:00',39),
        ('SH1017','JNPT','Kolkata','R03','C09','V305','DISRUPTED','HIGH','Heavy Machinery',0,'2024-06-11 00:00','2024-06-15 00:00','2024-06-17 00:00',48),
        ('SH1018','Mumbai','Goa','R08','C01','T211','DELAYED','LOW','Furniture',0,'2024-06-10 12:00','2024-06-11 00:00','2024-06-11 15:00',15),
        ('SH1019','Mumbai','Delhi','R01','C07','T212','DISRUPTED','HIGH','Perishable Foods',1,'2024-06-10 09:00','2024-06-11 03:00','2024-06-12 12:00',33),
        ('SH1020','Chennai','JNPT','R09','C06','V306','DISRUPTED','MEDIUM','Textiles',0,'2024-06-10 08:00','2024-06-13 00:00','2024-06-14 18:00',42),
        ('SH1021','Delhi','Kolkata','R10','C05','CN403','ON_TIME','LOW','Books',0,'2024-06-10 10:00','2024-06-11 08:00','2024-06-11 08:00',0),
        ('SH1022','Pune','Mumbai','R04','C08','T213','ON_TIME','HIGH','Electronic Components',0,'2024-06-10 11:00','2024-06-10 14:30','2024-06-10 14:30',0),
        ('SH1023','Mumbai','Ahmedabad','R05','C02','T214','DELAYED','MEDIUM','Plastic Granules',0,'2024-06-10 13:00','2024-06-10 21:00','2024-06-11 00:00',3),
        ('SH1024','Mumbai','Chennai','R02','C03','V307','DISRUPTED','CRITICAL','Blood Plasma',1,'2024-06-10 07:00','2024-06-12 11:00','2024-06-14 17:00',54),
        ('SH1025','Hyderabad','Mumbai','R16','C06','T218','ON_TIME','LOW','Textile Machinery',0,'2024-06-10 06:00','2024-06-11 04:00','2024-06-11 04:00',0),
        ('SH1026','Delhi','Mumbai','R17','C01','T219','ON_TIME','MEDIUM','Clothing',0,'2024-06-10 07:00','2024-06-11 07:00','2024-06-11 07:00',0),
        ('SH1027','Kolkata','Delhi','R18','C07','T220','DELAYED','MEDIUM','Steel Coils',0,'2024-06-10 05:00','2024-06-11 17:00','2024-06-12 05:00',12),
        ('SH1028','JNPT','Dubai','R07','C09','V309','AT_RISK','HIGH','Chemical Precursors',0,'2024-06-09 12:00','2024-06-14 12:00','2024-06-16 00:00',36),
        ('SH1029','Bangalore','Chennai','R14','C06','T221','ON_TIME','LOW','Coffee Beans',1,'2024-06-10 08:00','2024-06-10 14:00','2024-06-10 14:00',0),
        ('SH1030','Mumbai','Kolkata','R19','C05','CN406','DELAYED','HIGH','Automotive Components',0,'2024-06-10 04:00','2024-06-12 04:00','2024-06-12 20:00',16),
        ('SH1031','Delhi','Ahmedabad','R20','C02','T222','ON_TIME','LOW','Office Supplies',0,'2024-06-10 09:00','2024-06-11 01:00','2024-06-11 01:00',0),
        ('SH1032','Chennai','Hyderabad','R21','C06','T223','AT_RISK','MEDIUM','Electrical Cables',0,'2024-06-10 07:30','2024-06-10 15:30','2024-06-10 18:00',2),
        ('SH1033','Pune','Bangalore','R22','C08','T224','ON_TIME','LOW','Software Hardware',0,'2024-06-10 10:00','2024-06-11 08:00','2024-06-11 08:00',0),
        ('SH1034','Mumbai','Hyderabad','R23','C01','T225','DISRUPTED','HIGH','Pharma Raw Materials',1,'2024-06-10 06:00','2024-06-11 10:00','2024-06-12 04:00',18),
        ('SH1035','JNPT','Singapore','R24','C03','V310','ON_TIME','MEDIUM','Garments',0,'2024-06-09 00:00','2024-06-13 00:00','2024-06-13 00:00',0),
        ('SH1036','Kolkata','Mumbai','R19','C09','V311','DELAYED','MEDIUM','Iron Ore',0,'2024-06-09 06:00','2024-06-14 00:00','2024-06-15 12:00',36),
        ('SH1037','Bangalore','Mumbai','R25','C06','T226','ON_TIME','LOW','Processed Foods',0,'2024-06-10 05:00','2024-06-11 11:00','2024-06-11 11:00',0),
        ('SH1038','Delhi','Chennai','R26','C05','CN407','AT_RISK','HIGH','Medical Supplies',1,'2024-06-10 04:00','2024-06-11 16:00','2024-06-12 04:00',12),
        ('SH1039','Ahmedabad','Delhi','R27','C02','T227','ON_TIME','LOW','Agri Products',0,'2024-06-10 06:00','2024-06-10 18:00','2024-06-10 18:00',0),
        ('SH1040','Mumbai','Bangalore','R28','C07','T228','DELAYED','MEDIUM','Consumer Electronics',0,'2024-06-10 08:00','2024-06-11 14:00','2024-06-11 22:00',8),
        ('SH1041','Chennai','Mumbai','R02','C06','V312','DISRUPTED','HIGH','Granite Tiles',0,'2024-06-10 00:00','2024-06-13 00:00','2024-06-15 12:00',60),
        ('SH1042','Hyderabad','Delhi','R29','C01','T229','ON_TIME','MEDIUM','Engineering Parts',0,'2024-06-10 05:00','2024-06-11 17:00','2024-06-11 17:00',0),
        ('SH1043','Pune','Hyderabad','R30','C08','T230','ON_TIME','LOW','Tyres',0,'2024-06-10 09:00','2024-06-11 05:00','2024-06-11 05:00',0),
        ('SH1044','Mumbai','Kolkata','R19','C03','V313','AT_RISK','HIGH','Zinc Ingots',0,'2024-06-09 18:00','2024-06-14 00:00','2024-06-15 06:00',30),
        ('SH1045','Kolkata','Chennai','R31','C09','V314','ON_TIME','LOW','Jute Fibre',0,'2024-06-08 12:00','2024-06-14 12:00','2024-06-14 12:00',0),
        ('SH1046','Delhi','Kolkata','R10','C07','T231','DELAYED','MEDIUM','Furniture Parts',0,'2024-06-10 07:00','2024-06-11 19:00','2024-06-12 07:00',12),
        ('SH1047','Mumbai','Delhi','R11','C05','CN408','ON_TIME','HIGH','Security Equipment',0,'2024-06-10 03:00','2024-06-11 03:00','2024-06-11 03:00',0),
        ('SH1048','Bangalore','Kolkata','R32','C06','V315','DISRUPTED','MEDIUM','Silk Fabric',1,'2024-06-10 00:00','2024-06-15 00:00','2024-06-16 12:00',36),
        ('SH1049','JNPT','Kolkata','R03','C03','V316','DELAYED','HIGH','Wind Turbine Parts',0,'2024-06-09 00:00','2024-06-15 00:00','2024-06-18 00:00',72),
        ('SH1050','Ahmedabad','Mumbai','R13','C02','T232','ON_TIME','LOW','Cotton Bales',0,'2024-06-10 04:00','2024-06-10 14:00','2024-06-10 14:00',0),
        ('SH1051','Mumbai','Goa','R08','C01','T233','AT_RISK','MEDIUM','Tourist Goods',0,'2024-06-10 10:00','2024-06-10 22:00','2024-06-11 04:00',6),
        ('SH1052','Delhi','Jaipur','R06','C07','T234','ON_TIME','LOW','Handicrafts',0,'2024-06-10 11:00','2024-06-10 16:30','2024-06-10 16:30',0),
        ('SH1053','Chennai','Bangalore','R14','C06','T235','DELAYED','MEDIUM','Rubber Products',0,'2024-06-10 06:00','2024-06-10 12:00','2024-06-10 15:00',3),
        ('SH1054','Pune','Mumbai','R04','C08','T236','ON_TIME','LOW','Auto Ancillaries',0,'2024-06-10 12:00','2024-06-10 15:30','2024-06-10 15:30',0),
        ('SH1055','Mumbai','Ahmedabad','R05','C02','T237','DISRUPTED','HIGH','Solvent Chemicals',1,'2024-06-10 08:00','2024-06-10 16:00','2024-06-11 04:00',12),
        ('SH1056','Kolkata','JNPT','R33','C09','V317','ON_TIME','MEDIUM','Rice Exports',0,'2024-06-09 06:00','2024-06-13 06:00','2024-06-13 06:00',0),
        ('SH1057','Hyderabad','Bangalore','R34','C06','T238','ON_TIME','LOW','Pharmaceutical Packaging',0,'2024-06-10 07:00','2024-06-10 15:00','2024-06-10 15:00',0),
        ('SH1058','Delhi','Mumbai','R17','C01','T239','AT_RISK','HIGH','Precision Instruments',0,'2024-06-10 04:00','2024-06-11 04:00','2024-06-11 12:00',8),
        ('SH1059','Mumbai','Chennai','R02','C05','CN409','DELAYED','MEDIUM','Printing Machinery',0,'2024-06-10 03:00','2024-06-11 09:00','2024-06-11 21:00',12),
        ('SH1060','Bangalore','Delhi','R35','C07','T240','ON_TIME','LOW','Electronic Modules',0,'2024-06-10 02:00','2024-06-11 14:00','2024-06-11 14:00',0),
        ('SH1061','JNPT','Mumbai','R36','C03','V318','DISRUPTED','CRITICAL','Chemical Cargo',1,'2024-06-10 06:00','2024-06-11 18:00','2024-06-13 06:00',36),
        ('SH1062','Mumbai','Kolkata','R19','C09','V319','ON_TIME','MEDIUM','Fertilizer',0,'2024-06-10 00:00','2024-06-13 12:00','2024-06-13 12:00',0),
        ('SH1063','Delhi','Bangalore','R37','C05','CN410','AT_RISK','HIGH','Telecom Equipment',0,'2024-06-10 05:00','2024-06-11 21:00','2024-06-12 09:00',12),
        ('SH1064','Ahmedabad','Chennai','R38','C02','T241','DELAYED','MEDIUM','Marble Slabs',0,'2024-06-10 06:00','2024-06-12 06:00','2024-06-12 18:00',12),
        ('SH1065','Chennai','JNPT','R09','C06','V320','ON_TIME','LOW','Seafood Exports',1,'2024-06-10 04:00','2024-06-12 16:00','2024-06-12 16:00',0),
        ('SH1066','Hyderabad','Mumbai','R16','C01','T242','ON_TIME','LOW','Pharma Formulations',1,'2024-06-10 05:00','2024-06-11 05:00','2024-06-11 05:00',0),
        ('SH1067','Kolkata','Delhi','R18','C07','T243','DISRUPTED','HIGH','Coal Products',0,'2024-06-10 02:00','2024-06-11 14:00','2024-06-12 14:00',24),
        ('SH1068','Mumbai','Dubai','R07','C03','V321','AT_RISK','HIGH','Gems and Jewellery',0,'2024-06-09 12:00','2024-06-14 00:00','2024-06-15 12:00',36),
        ('SH1069','Bangalore','Mumbai','R25','C06','T244','DELAYED','LOW','Handicraft Goods',0,'2024-06-10 09:00','2024-06-11 15:00','2024-06-11 22:00',7),
        ('SH1070','Delhi','Ahmedabad','R20','C02','T245','ON_TIME','LOW','Paper Products',0,'2024-06-10 08:00','2024-06-10 20:00','2024-06-10 20:00',0),
        ('SH1071','Mumbai','Hyderabad','R23','C08','T246','ON_TIME','MEDIUM','Plastics',0,'2024-06-10 10:00','2024-06-11 06:00','2024-06-11 06:00',0),
        ('SH1072','Pune','Delhi','R12','C07','T247','DELAYED','HIGH','Defence Components',0,'2024-06-10 04:00','2024-06-11 16:00','2024-06-12 04:00',12),
        ('SH1073','JNPT','Singapore','R24','C09','V322','ON_TIME','MEDIUM','Spices',0,'2024-06-09 00:00','2024-06-14 00:00','2024-06-14 00:00',0),
        ('SH1074','Chennai','Kolkata','R31','C06','V323','DISRUPTED','MEDIUM','Tobacco Products',0,'2024-06-10 00:00','2024-06-15 00:00','2024-06-16 12:00',36),
        ('SH1075','Mumbai','Delhi','R01','C01','T248','DISRUPTED','CRITICAL','Insulin Vials',1,'2024-06-10 06:00','2024-06-11 00:00','2024-06-12 12:00',36)
    )");
}

void Database::seedFleetAssets() {
    exec("DELETE FROM fleet_assets");
    exec(R"(
        INSERT INTO fleet_assets VALUES
        ('T201','TRUCK','Mumbai','IN_TRANSIT',20000,'SH1001','2024-06-12 06:00',85,0),
        ('T202','TRUCK','Pune','IN_TRANSIT',18000,'SH1004','2024-06-10 10:30',60,0),
        ('T203','TRUCK','Mumbai','IN_TRANSIT',22000,'SH1005','2024-06-10 20:00',70,0),
        ('T204','TRUCK','Mumbai','IDLE',20000,'','',0,0),
        ('T205','TRUCK','Delhi','IN_TRANSIT',15000,'SH1007','2024-06-10 13:30',55,0),
        ('T206','TRUCK','Delhi','IN_TRANSIT',25000,'SH1010','2024-06-11 05:00',75,0),
        ('T207','TRUCK','Pune','IN_TRANSIT',20000,'SH1012','2024-06-11 08:00',40,0),
        ('T208','TRUCK','Ahmedabad','IN_TRANSIT',18000,'SH1013','2024-06-10 15:00',50,0),
        ('T209','TRUCK','Chennai','IN_TRANSIT',16000,'SH1014','2024-06-10 13:00',80,1),
        ('T210','TRUCK','Mumbai','IN_TRANSIT',20000,'SH1016','2024-06-12 14:00',90,1),
        ('T211','TRUCK','Mumbai','IDLE',18000,'','',0,0),
        ('T212','TRUCK','Mumbai','IN_TRANSIT',22000,'SH1019','2024-06-12 12:00',85,0),
        ('T213','TRUCK','Pune','IN_TRANSIT',15000,'SH1022','2024-06-10 14:30',65,0),
        ('T214','TRUCK','Mumbai','IN_TRANSIT',20000,'SH1023','2024-06-11 00:00',45,0),
        ('T215','TRUCK','Delhi','IDLE',25000,'','',0,0),
        ('T216','TRUCK','Bangalore','AVAILABLE',20000,'','2024-06-10 16:00',0,1),
        ('T217','TRUCK','Hyderabad','IDLE',18000,'','',0,0),
        ('V301','VESSEL','JNPT','IN_TRANSIT',500000,'SH1002','2024-06-16 12:00',70,0),
        ('V302','VESSEL','Mumbai','IN_TRANSIT',450000,'SH1003','2024-06-14 20:00',65,1),
        ('V303','VESSEL','Mumbai','DELAYED',400000,'SH1008','2024-06-22 00:00',40,0),
        ('V304','VESSEL','Chennai','IN_TRANSIT',350000,'SH1009','2024-06-14 12:00',80,1),
        ('V305','VESSEL','JNPT','IN_TRANSIT',600000,'SH1017','2024-06-17 00:00',75,0),
        ('V306','VESSEL','Chennai','IN_TRANSIT',300000,'SH1020','2024-06-14 18:00',55,0),
        ('V307','VESSEL','Mumbai','IN_TRANSIT',400000,'SH1024','2024-06-14 17:00',90,1),
        ('V308','VESSEL','Kolkata','IDLE',500000,'','',0,0),
        ('CN401','CONTAINER','Mumbai','IN_TRANSIT',30000,'SH1011','2024-06-11 06:00',80,1),
        ('CN402','CONTAINER','Mumbai','IN_TRANSIT',28000,'SH1015','2024-06-11 03:00',75,0),
        ('CN403','CONTAINER','Delhi','IN_TRANSIT',30000,'SH1021','2024-06-11 08:00',60,0),
        ('CN404','CONTAINER','Pune','IDLE',28000,'','',0,1),
        ('CN405','CONTAINER','Chennai','AVAILABLE',30000,'','2024-06-10 18:00',0,0)
    )");
}

void Database::seedColdChainConfig() {
    exec("DELETE FROM cold_chain_config");
    exec(R"(
        INSERT INTO cold_chain_config VALUES
        ('SH1001',  2.0,  8.0, 1.0, 3.0, 30),
        ('SH1009',  2.0,  8.0, 1.0, 3.0, 20),
        ('SH1011', 15.0, 25.0, 2.0, 5.0, 60),
        ('SH1014', -18.0, -15.0, 1.0, 3.0, 15),
        ('SH1016',  2.0,  8.0, 1.0, 3.0, 20),
        ('SH1019',  2.0, 10.0, 1.5, 4.0, 45),
        ('SH1024',  2.0,  6.0, 0.5, 2.0, 10)
    )");
}

void Database::seedSensorReadings() {
    exec("DELETE FROM sensor_readings");

    // Helper: insertions for each cold-chain shipment
    // SH1001 — Pharmaceuticals: has a WARNING excursion
    exec(R"(
        INSERT INTO sensor_readings (shipment_id,timestamp,temperature,humidity,sensor_status) VALUES
        ('SH1001','2024-06-10 06:00',4.2,62,'OK'),
        ('SH1001','2024-06-10 06:30',4.5,63,'OK'),
        ('SH1001','2024-06-10 07:00',5.1,64,'OK'),
        ('SH1001','2024-06-10 07:30',5.8,65,'OK'),
        ('SH1001','2024-06-10 08:00',6.2,66,'OK'),
        ('SH1001','2024-06-10 08:30',7.1,67,'OK'),
        ('SH1001','2024-06-10 09:00',8.4,68,'OK'),
        ('SH1001','2024-06-10 09:30',8.9,69,'OK'),
        ('SH1001','2024-06-10 10:00',8.2,68,'OK'),
        ('SH1001','2024-06-10 10:30',7.5,67,'OK'),
        ('SH1001','2024-06-10 11:00',6.8,66,'OK'),
        ('SH1001','2024-06-10 11:30',6.1,65,'OK')
    )");

    // SH1009 — Medical Devices: HIGH excursion
    exec(R"(
        INSERT INTO sensor_readings (shipment_id,timestamp,temperature,humidity,sensor_status) VALUES
        ('SH1009','2024-06-10 06:00',3.5,55,'OK'),
        ('SH1009','2024-06-10 06:30',4.0,56,'OK'),
        ('SH1009','2024-06-10 07:00',4.8,57,'OK'),
        ('SH1009','2024-06-10 07:30',6.2,58,'OK'),
        ('SH1009','2024-06-10 08:00',7.8,60,'OK'),
        ('SH1009','2024-06-10 08:30',9.4,62,'OK'),
        ('SH1009','2024-06-10 09:00',10.2,64,'OK'),
        ('SH1009','2024-06-10 09:30',11.1,65,'OK'),
        ('SH1009','2024-06-10 10:00',10.8,64,'OK'),
        ('SH1009','2024-06-10 10:30',9.6,63,'OK'),
        ('SH1009','2024-06-10 11:00',8.4,62,'OK'),
        ('SH1009','2024-06-10 11:30',7.2,60,'OK')
    )");

    // SH1011 — Chemicals: NORMAL
    exec(R"(
        INSERT INTO sensor_readings (shipment_id,timestamp,temperature,humidity,sensor_status) VALUES
        ('SH1011','2024-06-10 06:00',19.5,50,'OK'),
        ('SH1011','2024-06-10 06:30',20.1,51,'OK'),
        ('SH1011','2024-06-10 07:00',20.8,52,'OK'),
        ('SH1011','2024-06-10 07:30',21.2,52,'OK'),
        ('SH1011','2024-06-10 08:00',21.5,53,'OK'),
        ('SH1011','2024-06-10 08:30',22.0,53,'OK'),
        ('SH1011','2024-06-10 09:00',22.3,54,'OK'),
        ('SH1011','2024-06-10 09:30',21.9,54,'OK')
    )");

    // SH1014 — Frozen Foods: CRITICAL excursion
    exec(R"(
        INSERT INTO sensor_readings (shipment_id,timestamp,temperature,humidity,sensor_status) VALUES
        ('SH1014','2024-06-10 07:00',-17.8,45,'OK'),
        ('SH1014','2024-06-10 07:30',-17.5,46,'OK'),
        ('SH1014','2024-06-10 08:00',-16.8,47,'OK'),
        ('SH1014','2024-06-10 08:30',-14.2,48,'OK'),
        ('SH1014','2024-06-10 09:00',-11.5,50,'OK'),
        ('SH1014','2024-06-10 09:30',-9.8,52,'OK'),
        ('SH1014','2024-06-10 10:00',-10.2,51,'OK'),
        ('SH1014','2024-06-10 10:30',-12.1,50,'OK'),
        ('SH1014','2024-06-10 11:00',-14.5,48,'OK'),
        ('SH1014','2024-06-10 11:30',-15.8,47,'OK')
    )");

    // SH1016 — Vaccines: CRITICAL excursion (worst)
    exec(R"(
        INSERT INTO sensor_readings (shipment_id,timestamp,temperature,humidity,sensor_status) VALUES
        ('SH1016','2024-06-10 05:00',3.8,60,'OK'),
        ('SH1016','2024-06-10 05:30',4.2,61,'OK'),
        ('SH1016','2024-06-10 06:00',5.5,62,'OK'),
        ('SH1016','2024-06-10 06:30',7.8,64,'OK'),
        ('SH1016','2024-06-10 07:00',9.5,66,'OK'),
        ('SH1016','2024-06-10 07:30',11.8,68,'OK'),
        ('SH1016','2024-06-10 08:00',13.2,70,'OK'),
        ('SH1016','2024-06-10 08:30',14.5,72,'FAULT'),
        ('SH1016','2024-06-10 09:00',15.1,73,'FAULT'),
        ('SH1016','2024-06-10 09:30',13.8,71,'OK'),
        ('SH1016','2024-06-10 10:00',11.2,69,'OK'),
        ('SH1016','2024-06-10 10:30',9.0,67,'OK'),
        ('SH1016','2024-06-10 11:00',7.5,65,'OK'),
        ('SH1016','2024-06-10 11:30',6.2,63,'OK')
    )");

    // SH1019 — Perishable Foods: HIGH excursion
    exec(R"(
        INSERT INTO sensor_readings (shipment_id,timestamp,temperature,humidity,sensor_status) VALUES
        ('SH1019','2024-06-10 09:00',4.5,65,'OK'),
        ('SH1019','2024-06-10 09:30',5.2,66,'OK'),
        ('SH1019','2024-06-10 10:00',6.8,67,'OK'),
        ('SH1019','2024-06-10 10:30',8.5,68,'OK'),
        ('SH1019','2024-06-10 11:00',10.2,70,'OK'),
        ('SH1019','2024-06-10 11:30',11.8,71,'OK'),
        ('SH1019','2024-06-10 12:00',10.5,70,'OK'),
        ('SH1019','2024-06-10 12:30',9.0,69,'OK')
    )");

    // SH1024 — Blood Plasma: CRITICAL (most sensitive)
    exec(R"(
        INSERT INTO sensor_readings (shipment_id,timestamp,temperature,humidity,sensor_status) VALUES
        ('SH1024','2024-06-10 07:00',3.2,58,'OK'),
        ('SH1024','2024-06-10 07:30',3.8,59,'OK'),
        ('SH1024','2024-06-10 08:00',4.5,60,'OK'),
        ('SH1024','2024-06-10 08:30',5.9,61,'OK'),
        ('SH1024','2024-06-10 09:00',6.8,63,'OK'),
        ('SH1024','2024-06-10 09:30',7.4,64,'OK'),
        ('SH1024','2024-06-10 10:00',8.2,65,'OK'),
        ('SH1024','2024-06-10 10:30',8.5,66,'OK'),
        ('SH1024','2024-06-10 11:00',8.0,65,'OK'),
        ('SH1024','2024-06-10 11:30',7.2,64,'OK')
    )");
}

// ── Reset ─────────────────────────────────────────────────────────────────────
void Database::resetToSeedState() {
    exec("DELETE FROM disruptions");
    exec("DELETE FROM routes");
    exec("DELETE FROM carriers");
    exec("DELETE FROM shipments");
    exec("DELETE FROM fleet_assets");
    exec("DELETE FROM sensor_readings");
    exec("DELETE FROM cold_chain_config");
    seedData();
}

} // namespace db
