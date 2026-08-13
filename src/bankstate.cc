#include "bankstate.h"
#include <common.h>

namespace dramsim3 {

BankState::BankState(bool enable_pim_switch)
    : state_(State::CLOSED_ISOLATED),
      cmd_timing_(static_cast<int>(CommandType::SIZE)),
      open_row_(-1),
      row_hit_count_(0),
      enable_pim_switch_(enable_pim_switch),
      is_pim_mode_(false) {}

void BankState::SaveNearRowContext(NearRowContext& context) const {
    switch (state_) {
        case State::CLOSED_ISOLATED:
            context = NearRowContext();
            return;
        case State::OPEN_ISOLATED:
            context.is_open = true;
            context.open_row = open_row_;
            context.row_hit_count = row_hit_count_;
            return;
        case State::CONNECTING_FOR_ACT:
        case State::OPEN_CONNECTED:
        case State::CONNECTING_FOR_PRE:
        case State::PRECHARGING_CONNECTED:
        case State::SREF:
        case State::PD:
        case State::SIZE:
            std::cerr << "Cannot switch PIM mode from a non-stable bank state."
                      << std::endl;
            AbruptExit(__FILE__, __LINE__);
    }
}

void BankState::LoadNearRowContext(const NearRowContext& context) {
    state_ = context.is_open ? State::OPEN_ISOLATED
                             : State::CLOSED_ISOLATED;
    open_row_ = context.is_open ? context.open_row : -1;
    row_hit_count_ = context.is_open ? context.row_hit_count : 0;
    sequence_command_ = Command();
}

void BankState::SetPimMode(bool mode) {
    if (mode == is_pim_mode_) {
        return;
    }

    if (enable_pim_switch_) {
        NearRowContext& outgoing =
            is_pim_mode_ ? pim_context_ : cpu_context_;
        const NearRowContext& incoming = mode ? pim_context_ : cpu_context_;
        SaveNearRowContext(outgoing);
        LoadNearRowContext(incoming);
    }
    is_pim_mode_ = mode;
}


Command BankState::GetReadyCommand(const Command& cmd, uint64_t clk) const {
    CommandType required_type = CommandType::SIZE;
    switch (state_) {
        case State::CLOSED_ISOLATED:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                    required_type = enable_pim_switch_
                                        ? CommandType::TOGGLE_ON
                                        : CommandType::ACTIVATE;
                    break;
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                case CommandType::SREF_ENTER:
                    required_type = cmd.cmd_type;
                    break;
                case CommandType::ACTIVATE:
                case CommandType::PRECHARGE:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
                    std::cerr << "Unknown type!" << std::endl;
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::CONNECTING_FOR_ACT:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                case CommandType::SREF_ENTER:
                    required_type = CommandType::ACTIVATE;
                    break;
                case CommandType::ACTIVATE:
                case CommandType::PRECHARGE:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::OPEN_CONNECTED:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                case CommandType::SREF_ENTER:
                    required_type = CommandType::TOGGLE_OFF;
                    break;
                case CommandType::ACTIVATE:
                case CommandType::PRECHARGE:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::OPEN_ISOLATED:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                    if (cmd.Row() == open_row_) {
                        required_type = cmd.cmd_type;
                    } else {
                        required_type = enable_pim_switch_
                                            ? CommandType::TOGGLE_ON
                                            : CommandType::PRECHARGE;
                    }
                    break;
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                case CommandType::SREF_ENTER:
                    required_type = enable_pim_switch_
                                        ? CommandType::TOGGLE_ON
                                        : CommandType::PRECHARGE;
                    break;
                case CommandType::ACTIVATE:
                case CommandType::PRECHARGE:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
                    std::cerr << "Unknown type!" << std::endl;
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::CONNECTING_FOR_PRE:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                case CommandType::SREF_ENTER:
                    required_type = CommandType::PRECHARGE;
                    break;
                case CommandType::ACTIVATE:
                case CommandType::PRECHARGE:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
                    AbruptExit(__FILE__, __LINE__);
                    break;
            }
            break;
        case State::PRECHARGING_CONNECTED:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE:
                case CommandType::WRITE_PRECHARGE:
                    // The isolation transistor remains on after PRECHARGE,
                    // so a row-conflict request can ACTIVATE directly once
                    // the existing tRP constraint has elapsed.
                    required_type = CommandType::ACTIVATE;
                    break;
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                case CommandType::SREF_ENTER:
                    required_type = CommandType::TOGGLE_OFF;
                    break;
                case CommandType::ACTIVATE:
                case CommandType::PRECHARGE:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
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
                    required_type = CommandType::SREF_EXIT;
                    break;
                case CommandType::ACTIVATE:
                case CommandType::PRECHARGE:
                case CommandType::REFRESH_BANK:
                case CommandType::REFRESH:
                case CommandType::SREF_ENTER:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
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
    }

    if (required_type != CommandType::SIZE) {
        if (clk >= cmd_timing_[static_cast<int>(required_type)]) {
            Command ready(required_type, cmd.addr, cmd.hex_addr, cmd.is_pim,
                          cmd.transaction_id);
            if (state_ == State::CONNECTING_FOR_ACT ||
                state_ == State::OPEN_CONNECTED ||
                state_ == State::CONNECTING_FOR_PRE) {
                ready.addr = sequence_command_.addr;
                ready.hex_addr = sequence_command_.hex_addr;
                ready.is_pim = sequence_command_.is_pim;
                ready.transaction_id = sequence_command_.transaction_id;
            } else if (state_ == State::PRECHARGING_CONNECTED &&
                       required_type == CommandType::TOGGLE_OFF) {
                ready.addr = sequence_command_.addr;
                ready.hex_addr = sequence_command_.hex_addr;
                ready.is_pim = sequence_command_.is_pim;
                ready.transaction_id = sequence_command_.transaction_id;
            } else if (required_type == CommandType::PRECHARGE ||
                       (required_type == CommandType::TOGGLE_ON &&
                        state_ == State::OPEN_ISOLATED)) {
                ready.addr.row = open_row_;
            }
            return ready;
        }
    }
    return Command();
}

void BankState::UpdateState(const Command& cmd) {
    switch (state_) {
        case State::CLOSED_ISOLATED:
            switch (cmd.cmd_type) {
                case CommandType::REFRESH:
                case CommandType::REFRESH_BANK:
                    break;
                case CommandType::ACTIVATE:
                    if (enable_pim_switch_) {
                        AbruptExit(__FILE__, __LINE__);
                    }
                    state_ = State::OPEN_ISOLATED;
                    open_row_ = cmd.Row();
                    break;
                case CommandType::TOGGLE_ON:
                    if (!enable_pim_switch_) {
                        AbruptExit(__FILE__, __LINE__);
                    }
                    sequence_command_ = cmd;
                    state_ = State::CONNECTING_FOR_ACT;
                    break;
                case CommandType::SREF_ENTER:
                    state_ = State::SREF;
                    break;
                case CommandType::READ:
                case CommandType::WRITE:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::PRECHARGE:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::CONNECTING_FOR_ACT:
            switch (cmd.cmd_type) {
                case CommandType::ACTIVATE:
                    state_ = State::OPEN_CONNECTED;
                    open_row_ = cmd.Row();
                    row_hit_count_ = 0;
                    sequence_command_ = cmd;
                    break;
                case CommandType::READ:
                case CommandType::WRITE:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::PRECHARGE:
                case CommandType::REFRESH_BANK:
                case CommandType::REFRESH:
                case CommandType::SREF_ENTER:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::OPEN_CONNECTED:
            switch (cmd.cmd_type) {
                case CommandType::TOGGLE_OFF:
                    state_ = State::OPEN_ISOLATED;
                    sequence_command_ = Command();
                    break;
                case CommandType::READ:
                case CommandType::WRITE:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::ACTIVATE:
                case CommandType::PRECHARGE:
                case CommandType::REFRESH_BANK:
                case CommandType::REFRESH:
                case CommandType::SREF_ENTER:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::SIZE:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::OPEN_ISOLATED:
            switch (cmd.cmd_type) {
                case CommandType::READ:
                case CommandType::WRITE:
                    row_hit_count_++;
                    break;
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::PRECHARGE:
                    if (enable_pim_switch_) {
                        AbruptExit(__FILE__, __LINE__);
                    }
                    state_ = State::CLOSED_ISOLATED;
                    open_row_ = -1;
                    row_hit_count_ = 0;
                    break;
                case CommandType::TOGGLE_ON:
                    if (!enable_pim_switch_) {
                        AbruptExit(__FILE__, __LINE__);
                    }
                    sequence_command_ = cmd;
                    state_ = State::CONNECTING_FOR_PRE;
                    break;
                case CommandType::ACTIVATE:
                case CommandType::REFRESH_BANK:
                case CommandType::REFRESH:
                case CommandType::SREF_ENTER:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::CONNECTING_FOR_PRE:
            switch (cmd.cmd_type) {
                case CommandType::PRECHARGE:
                    state_ = State::PRECHARGING_CONNECTED;
                    break;
                case CommandType::READ:
                case CommandType::WRITE:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::ACTIVATE:
                case CommandType::REFRESH_BANK:
                case CommandType::REFRESH:
                case CommandType::SREF_ENTER:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::PRECHARGING_CONNECTED:
            switch (cmd.cmd_type) {
                case CommandType::ACTIVATE:
                    state_ = State::OPEN_CONNECTED;
                    open_row_ = cmd.Row();
                    row_hit_count_ = 0;
                    sequence_command_ = cmd;
                    break;
                case CommandType::TOGGLE_OFF:
                    state_ = State::CLOSED_ISOLATED;
                    open_row_ = -1;
                    row_hit_count_ = 0;
                    sequence_command_ = Command();
                    break;
                case CommandType::READ:
                case CommandType::WRITE:
                case CommandType::READ_PRECHARGE:
                case CommandType::WRITE_PRECHARGE:
                case CommandType::PRECHARGE:
                case CommandType::REFRESH_BANK:
                case CommandType::REFRESH:
                case CommandType::SREF_ENTER:
                case CommandType::SREF_EXIT:
                case CommandType::TOGGLE_ON:
                case CommandType::SIZE:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::SREF:
            switch (cmd.cmd_type) {
                case CommandType::SREF_EXIT:
                    state_ = State::CLOSED_ISOLATED;
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
                case CommandType::TOGGLE_ON:
                case CommandType::TOGGLE_OFF:
                case CommandType::SIZE:
                    AbruptExit(__FILE__, __LINE__);
            }
            break;
        case State::PD:
        case State::SIZE:
            AbruptExit(__FILE__, __LINE__);
    }
    return;
}

void BankState::UpdateTiming(CommandType cmd_type, uint64_t time) {
    // std::cerr << "timings\n"
    //           << "ACT:" << cmd_timing_[static_cast<int>(CommandType::ACTIVATE)] << std::endl
    //           << "PRE:" << cmd_timing_[static_cast<int>(CommandType::PRECHARGE)] << std::endl
    //           << "WRITE:" << cmd_timing_[static_cast<int>(CommandType::WRITE)] << std::endl
    //           << "READ:" << cmd_timing_[static_cast<int>(CommandType::READ)] << std::endl
    //           << "READ_PRE:" << cmd_timing_[static_cast<int>(CommandType::READ_PRECHARGE)] << std::endl
    //           << "WRITE_PRE:" << cmd_timing_[static_cast<int>(CommandType::WRITE_PRECHARGE)] << std::endl
    //           << std::endl;
    // if (cmd_type == CommandType::WRITE_PRECHARGE) {
    //     std::cerr << "Write precharge found at time " << time << std::endl;
    //     cmd_type = CommandType::WRITE;
    // }
    // if (cmd_type == CommandType::READ_PRECHARGE) {
    //     std::cerr << "Read precharge found at time " << time << std::endl;
    //     cmd_type = CommandType::READ;
    // }
    // if (cmd_type == CommandType::READ) {
    //     std::cerr << "Read [row open] found at time " << time << std::endl;
    // }
    // if (cmd_type == CommandType::WRITE) {
    //     std::cerr << "Write [row open] found at time " << time << std::endl;
    // }
    cmd_timing_[static_cast<int>(cmd_type)] =
        std::max(cmd_timing_[static_cast<int>(cmd_type)], time);
    return;
}

}  // namespace dramsim3
