#ifndef __UPMEM_AUTOMATA_H
#define __UPMEM_AUTOMATA_H

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <queue>

// States of the UPMEM execution lifecycle
enum class UpmemState {
    IDLE,
    RUNNING,
    PAUSE,
};

enum class UpmemCommand{
    PAUSE,
    RESUME,
    START,
    QUERY,
    UNDEF,
};

class UpmemAutomata {
    public:
        UpmemAutomata(uint64_t max_cycles, int id);

        // Call UpdateState inside ClockTick and return the current state only when cmd is PIM_STATE_QUERY;
        // return std::nullopt for all other commands or simple ticks.
        void ClockTick();

        // for testing and debugging
        UpmemState GetState()     const { return state_; }
        uint64_t   GetCounter()   const { return counter_; }
        uint64_t   GetClock()     const { return clk_; }
        uint64_t   GetMaxCycles() const { return max_cycles_; }
        void submit(UpmemCommand cmd);

    private:
        int id;
        UpmemState state_;
        uint64_t   counter_;
        uint64_t   clk_;
        uint64_t   max_cycles_; // threshold that triggers PROGRAM_END

        void UpdateState();
        std::queue<UpmemCommand> cmd_q;
};
#endif  // __UPMEM_AUTOMATA_H
