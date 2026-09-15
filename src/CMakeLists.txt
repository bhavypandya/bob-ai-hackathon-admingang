cmake_minimum_required(VERSION 3.16)
project(SupplyChainAssistant VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ─── Find packages ─────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)

# ─── SQLite3 ───────────────────────────────────────────────────────────────────
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    message(STATUS "System SQLite3 not found — using bundled amalgamation")
    add_library(sqlite3_bundled STATIC
        third_party/sqlite/sqlite3.c
    )
    target_include_directories(sqlite3_bundled PUBLIC third_party/sqlite)
    target_compile_options(sqlite3_bundled PRIVATE -w)
    set(SQLITE_LIB sqlite3_bundled)
    set(SQLITE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite)
else()
    message(STATUS "Using system SQLite3: ${SQLite3_INCLUDE_DIRS}")
    set(SQLITE_LIB SQLite::SQLite3)
    set(SQLITE_INCLUDE_DIR ${SQLite3_INCLUDE_DIRS})
endif()

# ─── nlohmann/json ─────────────────────────────────────────────────────────────
# json.hpp is at third_party/nlohmann/json.hpp → include with -I third_party
set(JSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# ─── Common include dirs ───────────────────────────────────────────────────────
set(COMMON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/backend
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/api
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/database
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/models
    ${CMAKE_CURRENT_SOURCE_DIR}/backend/services
    ${JSON_INCLUDE_DIR}
    ${SQLITE_INCLUDE_DIR}
)

# ─── Backend executable ────────────────────────────────────────────────────────
set(BACKEND_SOURCES
    backend/main.cpp
    backend/database/database.cpp
    backend/services/disruption_service.cpp
    backend/services/shipment_service.cpp
    backend/services/route_service.cpp
    backend/services/carrier_service.cpp
    backend/services/fleet_service.cpp
    backend/services/cold_chain_service.cpp
    backend/services/recommendation_service.cpp
    backend/services/simulation_service.cpp
    backend/api/api_handler.cpp
)

add_executable(supply_chain_backend ${BACKEND_SOURCES})
target_include_directories(supply_chain_backend PRIVATE ${COMMON_INCLUDES})
target_link_libraries(supply_chain_backend PRIVATE ${SQLITE_LIB} Threads::Threads)

if(WIN32)
    target_link_libraries(supply_chain_backend PRIVATE ws2_32 mswsock)
endif()

if(MSVC)
    target_compile_options(supply_chain_backend PRIVATE /W3 /wd4244 /wd4267)
else()
    target_compile_options(supply_chain_backend PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# ─── Test executable ───────────────────────────────────────────────────────────
enable_testing()

set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_disruption.cpp
    tests/test_routing.cpp
    tests/test_carrier.cpp
    tests/test_fleet.cpp
    tests/test_cold_chain.cpp
    backend/database/database.cpp
    backend/services/disruption_service.cpp
    backend/services/shipment_service.cpp
    backend/services/route_service.cpp
    backend/services/carrier_service.cpp
    backend/services/fleet_service.cpp
    backend/services/cold_chain_service.cpp
    backend/services/recommendation_service.cpp
    backend/services/simulation_service.cpp
    backend/api/api_handler.cpp
)

add_executable(run_tests ${TEST_SOURCES})
target_include_directories(run_tests PRIVATE ${COMMON_INCLUDES})
target_compile_definitions(run_tests PRIVATE TESTING_MODE)
target_link_libraries(run_tests PRIVATE ${SQLITE_LIB} Threads::Threads)

if(WIN32)
    target_link_libraries(run_tests PRIVATE ws2_32 mswsock)
endif()

if(MSVC)
    target_compile_options(run_tests PRIVATE /W3 /wd4244 /wd4267)
else()
    target_compile_options(run_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

add_test(NAME UnitTests COMMAND run_tests)

# ─── Copy frontend to build dir ────────────────────────────────────────────────
add_custom_command(TARGET supply_chain_backend POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/frontend
        ${CMAKE_BINARY_DIR}/bin/frontend
    COMMENT "Copying frontend files to build output"
)

message(STATUS "")
message(STATUS "=== Supply Chain Assistant Build ===")
message(STATUS "Build type:      ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard:    ${CMAKE_CXX_STANDARD}")
message(STATUS "SQLite lib:      ${SQLITE_LIB}")
message(STATUS "JSON include:    ${JSON_INCLUDE_DIR}")
message(STATUS "Output dir:      ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "")
message(STATUS "Run after build:")
message(STATUS "  cd build/bin && ./supply_chain_backend")
message(STATUS "  Then open: http://localhost:8080")
message(STATUS "")
