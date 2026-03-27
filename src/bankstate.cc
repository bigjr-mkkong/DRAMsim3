#include "bankstate.h"
#include "common.h"

namespace dramsim3 {

BankState::BankState()
    : state_(State::CLOSED),
      cmd_timing_(static_cast<int>(CommandType::SIZE)),
      open_row_(-1),
      row_hit_count_(0) {
    cmd_timing_[static_cast<int>(CommandType::READ)] = 0;
    cmd_timing_[static_cast<int>(CommandType::READ_PRECHARGE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::WRITE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::WRITE_PRECHARGE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::ACTIVATE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::PRECHARGE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::REFRESH)] = 0;
    cmd_timing_[static_cast<int>(CommandType::SREF_ENTER)] = 0;
    cmd_timing_[static_cast<int>(CommandType::SREF_EXIT)] = 0;
    cmd_timing_[static_cast<int>(CommandType::PIM_START)] = 0;
    cmd_timing_[static_cast<int>(CommandType::PIM_PAUSE)] = 0;
    cmd_timing_[static_cast<int>(CommandType::PIM_RESUME)] = 0;
    cmd_timing_[static_cast<int>(CommandType::PIM_STATE_QUERY)] = 0;
}


Command BankState::GetReadyCommand(const Command& cmd, uint64_t clk) const {
    CommandType required_type = CommandType::SIZE;
    switch (state_) {
        case State::CLOSED:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                    required_type = CommandType::ACTIVATE;
                    break;
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                case CommandType::SREF_ENTER:
                case CommandType::PIM_START:
                case CommandType::PIM_PAUSE:
                case CommandType::PIM_STATE_QUERY:
                    required_type = cmd.cmd_type;
                    break;
                default:
                    std::cerr << "Unknown type!" << std::endl;
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::OPEN:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                    if (cmd.Row() == open_row_) {
                        required_type = cmd.cmd_type;
                    } else {
                        required_type = CommandType::PRECHARGE;
                    }
                    break;
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                case CommandType::SREF_ENTER:
                case CommandType::PIM_START:
                case CommandType::PIM_PAUSE:
                    required_type = CommandType::PRECHARGE;
                    break;
                case CommandType::PIM_STATE_QUERY:
                    required_type = cmd.cmd_type;
                    break;
                default:
                    std::cerr << "Unknown type!" << std::endl;
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::SREF:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::PIM_START:
                case CommandType::PIM_PAUSE:
                case CommandType::PIM_RESUME:
                    required_type = CommandType::SREF_EXIT;
                    break;
                case CommandType::PIM_STATE_QUERY:
                    required_type = cmd.cmd_type;
                    break;
                default:
                    std::cerr << "Unknown type!" << std::endl;
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::PD:
        case State::SIZE:
            std::cerr << "In unknown state" << std::endl;
            AbruptExit(__FILE__, __LINE__);
            break;

        case State::PAUSING:
            switch (cmd.cmd_type) {
                case CommandType::PIM_STATE_QUERY:
                case CommandType::PIM_RESUME:
                case CommandType::PIM_PAUSE:
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                    required_type = cmd.cmd_type;
                    break;
                default:
                    std::cerr << "Unknown type in PAUSING state!" << std::endl;
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
    }

    if (required_type != CommandType::SIZE) {
        if (clk >= cmd_timing_[static_cast<int>(required_type)]) {
            return Command(required_type, cmd.addr, cmd.hex_addr);
        }
    }
    return Command();
}

void BankState::UpdateState(const Command& cmd) {
    switch (state_) {
        case State::OPEN:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::WRITE:
                    row_hit_count_++;
                    break;
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::PRECHARGE:
                    state_ = State::CLOSED;
                    open_row_ = -1;
                    row_hit_count_ = 0;
                    break;
                case CommandType::PIM_STATE_QUERY:
                    break;
                case CommandType::PIM_PAUSE:
                case CommandType::PIM_RESUME:
                case CommandType::PIM_START:
                case CommandType::ACTIVATE:
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                case CommandType::SREF_ENTER:
                case CommandType::SREF_EXIT:
                default:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::CLOSED:
            switch (cmd.cmd_type) {
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                    break;
                case CommandType::ACTIVATE:
                    state_ = State::OPEN;
                    open_row_ = cmd.Row();
                    break;
                case CommandType::SREF_ENTER:
                    state_ = State::SREF;
                    break;
                case CommandType::PIM_STATE_QUERY:
                    break;
                case CommandType::PIM_START:
                case CommandType::PIM_PAUSE:
                    prev_state = state_;
                    state_ = State::PAUSING;
                    break;
                case CommandType::READ:
                case CommandType::WRITE:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::PRECHARGE:
                case CommandType::SREF_EXIT:
                case CommandType::PIM_RESUME:
                default:
                    std::cout << cmd << std::endl;
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::SREF:
            switch (cmd.cmd_type) {
                case CommandType::PIM_STATE_QUERY:
                    break;
                case CommandType::SREF_EXIT:
                    state_ = State::CLOSED;
                    break;
                case CommandType::READ:
                case CommandType::WRITE:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::ACTIVATE:
                case CommandType::PRECHARGE:
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                case CommandType::SREF_ENTER:
                case CommandType::PIM_START:
                case CommandType::PIM_PAUSE:
                case CommandType::PIM_RESUME:
                default:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::PAUSING:
            switch (cmd.cmd_type) {
                case CommandType::PIM_RESUME:
                    state_ = prev_state;
                    open_row_ = prev_open_row;
                    break;
                default:
                    //Do Nothing
                    break;
                // case CommandType::PIM_STATE_QUERY:
                // case CommandType::REFRESH:
                // case CommandType::REFRESH_BANK:
                //     break;
                // case CommandType::PIM_START:
                // case CommandType::PIM_PAUSE:
                // case CommandType::ACTIVATE:
                // case CommandType::SREF_ENTER:
                // case CommandType::READ:
                // case CommandType::WRITE:
                // case CommandType::READ_PRECHARGE:
                // case CommandType::WRITE_PRECHARGE:
                // case CommandType::PRECHARGE:
                // case CommandType::SREF_EXIT:
                // default:
                //     std::cout << cmd << std::endl;
                //     AbruptExit(__FILE__, __LINE__);
            }
            break;
        default:
            AbruptExit(__FILE__, __LINE__);
    }
    return;
}

void BankState::UpdateTiming(CommandType cmd_type, uint64_t time) {
    cmd_timing_[static_cast<int>(cmd_type)] =
        std::max(cmd_timing_[static_cast<int>(cmd_type)], time);
    return;
}

}  // namespace dramsim3
