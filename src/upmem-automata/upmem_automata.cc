#include "upmem_automata.h"


std::string StateToString(UpmemState state) {
    switch (state) {
        case UpmemState::IDLE:    return "IDLE";
        case UpmemState::RUNNING: return "RUNNING";
        case UpmemState::PAUSE:   return "PAUSE";
        default:                  return "UNKNOWN";
    }
}

std::string CommandToString(UpmemCommand cmd) {
    switch (cmd) {
        case UpmemCommand::START:       return "PIM_START";
        case UpmemCommand::PAUSE:       return "PIM_PAUSE";
        case UpmemCommand::RESUME:      return "PIM_RESUME";
        case UpmemCommand::QUERY: return "PIM_STATE_QUERY";
        default:                           return "UNKNOWN";
    }
}

UpmemAutomata::UpmemAutomata(uint64_t max_cycles, int id)
    : state_(UpmemState::IDLE),
      counter_(0),
      clk_(0),
      max_cycles_(max_cycles),
        id(id) {}

void UpmemAutomata::ClockTick() {
    // Advance the clock and update the state based on the input command if any.
    clk_++;
    std::cout<<clk_<<std::endl;
    UpdateState();
}


void UpmemAutomata::submit(UpmemCommand cmd){
    cmd_q.push(cmd);
}

void UpmemAutomata::UpdateState() {
    UpmemCommand cmd = UpmemCommand::UNDEF;
    if(!cmd_q.empty()) {
        cmd = cmd_q.front();
        cmd_q.pop();
    }

    switch (state_) {
        // At each state, we first check if there is input command
        // if no command, we just keep the current state
        // and move on to the counter increment logic(only in RUNNING state).

        case UpmemState::IDLE:
            if (cmd == UpmemCommand::UNDEF) break;
            switch (cmd) {
                case UpmemCommand::START:
                    state_ = UpmemState::RUNNING;
                    break;
                case UpmemCommand::PAUSE:
                case UpmemCommand::RESUME:
                case UpmemCommand::QUERY: // IDLE does not support state query according to proposal
                    std::cerr << "[UPMEM] Warning: " << CommandToString(cmd)
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
            if (cmd == UpmemCommand::UNDEF) break;
            switch (cmd) {
                case UpmemCommand::QUERY:
                    // no state change; counter increments below
                    std::cout<<"PIM executed: "<<clk_<<" cycles"<<std::endl;
                    break;
                case UpmemCommand::PAUSE:
                    state_ = UpmemState::PAUSE;
                    break;
                case UpmemCommand::START:
                case UpmemCommand::RESUME:
                    std::cerr << "[UPMEM]"<<" Warning: " << CommandToString(cmd)
                              << " is invalid in RUNNING state at clk=" << clk_
                              << std::endl;
                    break;
                default:
                    std::cerr << "[UPMEM] Warning: non-PIM command received at clk="
                              << clk_ << std::endl;
                    break;
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
            if (cmd == UpmemCommand::UNDEF) break;
            switch (cmd) {
                case UpmemCommand::RESUME:
                    state_ = UpmemState::RUNNING;
                    break;
                case UpmemCommand::QUERY:
                    std::cout<<"[]UPMEM] clk val: "<<clk_<<std::endl;
                    break;
                case UpmemCommand::START:
                case UpmemCommand::PAUSE:
                    std::cerr << "[UPMEM] Warning: " << CommandToString(cmd)
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
    return;
}
