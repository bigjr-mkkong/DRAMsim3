#include "upmem_automata.h"

namespace upmem {

using dramsim3::CommandType;

std::string StateToString(UpmemState state) {
    switch (state) {
        case UpmemState::IDLE:    return "IDLE";
        case UpmemState::RUNNING: return "RUNNING";
        case UpmemState::PAUSE:   return "PAUSE";
        default:                  return "UNKNOWN";
    }
}

std::string CommandToString(CommandType cmd) {
    switch (cmd) {
        case CommandType::PIM_START:       return "PIM_START";
        case CommandType::PIM_PAUSE:       return "PIM_PAUSE";
        case CommandType::PIM_RESUME:      return "PIM_RESUME";
        case CommandType::PIM_STATE_QUERY: return "PIM_STATE_QUERY";
        default:                           return "UNKNOWN";
    }
}

UpmemAutomata::UpmemAutomata(uint64_t max_cycles)
    : state_(UpmemState::IDLE),
      counter_(0),
      clk_(0),
      max_cycles_(max_cycles) {}

std::optional<UpmemState> UpmemAutomata::ClockTick(std::optional<CommandType> cmd) {
    // Advance the clock and update the state based on the input command if any.
    clk_++;
    return UpdateState(cmd);
}

std::optional<UpmemState> UpmemAutomata::UpdateState(std::optional<CommandType> cmd) {
    bool is_query = cmd.has_value() && cmd.value() == CommandType::PIM_STATE_QUERY;

    switch (state_) {
        // At each state, we first check if there is input command
        // if no command, we just keep the current state
        // and move on to the counter increment logic(only in RUNNING state).

        case UpmemState::IDLE:
            if (!cmd.has_value()) break;
            switch (cmd.value()) {
                case CommandType::PIM_START:
                    state_ = UpmemState::RUNNING;
                    break;
                case CommandType::PIM_PAUSE:
                case CommandType::PIM_RESUME:
                case CommandType::PIM_STATE_QUERY: // IDLE does not support state query according to proposal
                    std::cerr << "[UPMEM] Warning: " << CommandToString(cmd.value())
                              << " is invalid in IDLE state at clk=" << clk_
                              << std::endl;
                    break;
                default:
                    std::cerr << "[UPMEM] Warning: non-PIM command received at clk="
                              << clk_ << std::endl;
                    break;
            }
            break;

        case UpmemState::RUNNING:
            if (cmd.has_value()) {
                switch (cmd.value()) {
                    case CommandType::PIM_STATE_QUERY:
                        // no state change; counter increments below
                        break;
                    case CommandType::PIM_PAUSE:
                        state_ = UpmemState::PAUSE;
                        break;
                    case CommandType::PIM_START:
                    case CommandType::PIM_RESUME:
                        std::cerr << "[UPMEM] Warning: " << CommandToString(cmd.value())
                                  << " is invalid in RUNNING state at clk=" << clk_
                                  << std::endl;
                        break;
                    default:
                        std::cerr << "[UPMEM] Warning: non-PIM command received at clk="
                                  << clk_ << std::endl;
                        break;
                }
            }
            // Counter increments every tick we remain in RUNNING,
            if (state_ == UpmemState::RUNNING) {
                counter_++;
                if (counter_ >= max_cycles_) {
                    std::cout << "[UPMEM] PROGRAM_END: counter=" << counter_
                              << " reached max_cycles=" << max_cycles_
                              << " at clk=" << clk_ << std::endl;
                    state_ = UpmemState::IDLE;
                    counter_ = 0;
                }
            }
            break;

        case UpmemState::PAUSE:
            if (!cmd.has_value()) break;
            switch (cmd.value()) {
                case CommandType::PIM_RESUME:
                    state_ = UpmemState::RUNNING;
                    break;
                case CommandType::PIM_STATE_QUERY:
                    // counter frozen; no state change
                    break;
                case CommandType::PIM_START:
                case CommandType::PIM_PAUSE:
                    std::cerr << "[UPMEM] Warning: " << CommandToString(cmd.value())
                              << " is invalid in PAUSE state at clk=" << clk_
                              << std::endl;
                    break;
                default:
                    std::cerr << "[UPMEM] Warning: invalid PIM command received at clk="
                              << clk_ << std::endl;
                    break;
            }
            break;
    }
    // Only return state for STATE_QUERY; all other ticks return nullopt
    return is_query ? std::optional<UpmemState>(state_) : std::nullopt;
}

}  // namespace upmem
