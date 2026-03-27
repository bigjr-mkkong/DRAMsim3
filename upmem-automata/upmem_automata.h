#ifndef __UPMEM_AUTOMATA_H
#define __UPMEM_AUTOMATA_H

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include "../src/common.h"

namespace upmem {

using dramsim3::CommandType;

// States of the UPMEM execution lifecycle
enum class UpmemState {
    IDLE,
    RUNNING,
    PAUSE,
};

std::string StateToString(UpmemState state);

std::string CommandToString(CommandType cmd);

class UpmemAutomata {
    public:
        explicit UpmemAutomata(uint64_t max_cycles);

        // Call UpdateState inside ClockTick and return the current state only when cmd is PIM_STATE_QUERY;
        // return std::nullopt for all other commands or simple ticks.
        std::optional<UpmemState> ClockTick(std::optional<CommandType> cmd = std::nullopt);

        // for testing and debugging
        UpmemState GetState()     const { return state_; }
        uint64_t   GetCounter()   const { return counter_; }
        uint64_t   GetClock()     const { return clk_; }
        uint64_t   GetMaxCycles() const { return max_cycles_; }

    private:
        UpmemState state_;
        uint64_t   counter_;
        uint64_t   clk_;
        uint64_t   max_cycles_; // threshold that triggers PROGRAM_END

        std::optional<UpmemState> UpdateState(std::optional<CommandType> cmd);
};

}  // namespace upmem

#endif  // __UPMEM_AUTOMATA_H
