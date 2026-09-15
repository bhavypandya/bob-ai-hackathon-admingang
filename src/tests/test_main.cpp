/*
 * Test runner — simple TAP-like output
 * No external test framework needed
 */
#include <iostream>
#include <string>
#include <functional>
#include <vector>
#include <stdexcept>

int g_passed = 0;
int g_failed = 0;

void check(bool condition, const std::string& test_name) {
    if (condition) {
        std::cout << "  ✓  " << test_name << "\n";
        g_passed++;
    } else {
        std::cout << "  ✗  " << test_name << "  <<< FAIL\n";
        g_failed++;
    }
}

// Forward declarations
void testDisruption();
void testRouting();
void testCarrier();
void testFleet();
void testColdChain();

int main() {
    std::cout << "\n=== Supply Chain Assistant — Unit Tests ===\n\n";

    try {
        testDisruption();
        testRouting();
        testCarrier();
        testFleet();
        testColdChain();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 1;
    }

    std::cout << "\n────────────────────────────────────\n";
    std::cout << "Results: " << g_passed << " passed, " << g_failed << " failed\n\n";
    return g_failed > 0 ? 1 : 0;
}
