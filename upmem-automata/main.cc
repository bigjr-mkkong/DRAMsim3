#include <iostream>
#include <optional>
#include <vector>
#include "upmem_automata.h"

using namespace upmem;
using dramsim3::CommandType;

// Issue a command and print the transition
static void Issue(UpmemAutomata& fsm, CommandType cmd) {
    UpmemState before = fsm.GetState();
    fsm.ClockTick(cmd);

    std::cout << "clk=" << fsm.GetClock()
              << "  cmd=" << CommandToString(cmd)
              << "  state: " << StateToString(before)
              << " -> " << StateToString(fsm.GetState())
              << "  counter=" << fsm.GetCounter()
              << std::endl;
}

// Tick with no command
static void Tick(UpmemAutomata& fsm) {
    UpmemState before = fsm.GetState();
    fsm.ClockTick();

    std::cout << "clk=" << fsm.GetClock()
              << "  cmd=(none)"
              << "  state: " << StateToString(before)
              << " -> " << StateToString(fsm.GetState())
              << "  counter=" << fsm.GetCounter()
              << std::endl;
}

// test PIM_STATE_QUERY: the only way to get state back from ClockTick
static void Query(UpmemAutomata& fsm) {
    auto result = fsm.ClockTick(CommandType::PIM_STATE_QUERY);

    std::cout << "clk=" << fsm.GetClock()
              << "  cmd=PIM_STATE_QUERY"
              << "  => state=" << StateToString(result.value())
              << "  counter=" << fsm.GetCounter()
              << std::endl;
}

int main() {
    // Demo 1: mix of explicit commands, queries and ticks
    const uint64_t MAX_CYCLES = 6;
    std::cout << "=== Demo 1: commands + bare ticks (max_cycles="
              << MAX_CYCLES << ") ===" << std::endl;

    UpmemAutomata fsm(MAX_CYCLES);

    Issue(fsm, CommandType::PIM_START);   // IDLE -> RUNNING
    Tick(fsm);                            // counter=1
    Tick(fsm);                            // counter=2
    Query(fsm);                           // returns RUNNING, counter=3
    Issue(fsm, CommandType::PIM_PAUSE);   // RUNNING -> PAUSE
    Tick(fsm);                            // counter frozen
    Tick(fsm);                            // counter frozen
    Query(fsm);                           // returns PAUSE, counter still 3
    Issue(fsm, CommandType::PIM_RESUME);  // PAUSE -> RUNNING
    Tick(fsm);                            // counter=4
    Tick(fsm);                            // counter=5
    Tick(fsm);                            // counter=6 -> PROGRAM_END -> IDLE

    std::cout << "Final state: " << StateToString(fsm.GetState()) << std::endl;

    // Demo 2: ClockTick returns nullopt for non-query commands
    std::cout << std::endl
              << "=== Demo 2: return value is nullopt except for STATE_QUERY ===" << std::endl;

    UpmemAutomata fsm2(10);
    auto r1 = fsm2.ClockTick(CommandType::PIM_START);
    std::cout << "PIM_START  returned: "
              << (r1.has_value() ? StateToString(r1.value()) : "nullopt") << std::endl;

    auto r2 = fsm2.ClockTick();
    std::cout << "bare tick  returned: "
              << (r2.has_value() ? StateToString(r2.value()) : "nullopt") << std::endl;

    auto r3 = fsm2.ClockTick(CommandType::PIM_STATE_QUERY);
    std::cout << "STATE_QUERY returned: "
              << (r3.has_value() ? StateToString(r3.value()) : "nullopt") << std::endl;

    return 0;
}
