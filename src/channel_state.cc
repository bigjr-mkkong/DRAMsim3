#include "channel_state.h"
#include <ios>
#include <assert.h>

namespace dramsim3 {
ChannelState::ChannelState(const Config& config, const Timing& timing)
    : rank_idle_cycles(config.ranks, 0),
      config_(config),
      timing_(timing),
      rank_is_sref_(config.ranks, false),
      four_aw_(config_.ranks, std::vector<uint64_t>()),
      thirty_two_aw_(config_.ranks, std::vector<uint64_t>()) {
    bank_states_.reserve(config_.ranks);
    for (auto i = 0; i < config_.ranks; i++) {
        auto rank_states = std::vector<std::vector<BankState>>();
        rank_states.reserve(config_.bankgroups);
        for (auto j = 0; j < config_.bankgroups; j++) {
            auto bg_states =
                std::vector<BankState>(
                    config_.banks_per_group,
                    BankState(config_.enable_pim_switch));
            rank_states.push_back(bg_states);
        }
        bank_states_.push_back(rank_states);
    }
}

void ChannelState::SetPimMode(bool mode) {
    if (mode == is_pim_mode_) {
        return;
    }

    for (auto& rank_states : bank_states_) {
        for (auto& bankgroup_states : rank_states) {
            for (auto& bank_state : bankgroup_states) {
                bank_state.SetPimMode(mode);
            }
        }
    }
    is_pim_mode_ = mode;
}

bool ChannelState::AreNearRowsStable() const {
    for (const auto& rank : bank_states_) {
        for (const auto& bank_group : rank) {
            for (const auto& bank : bank_group) {
                if (!bank.IsNearRowStable()) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool ChannelState::IsAllBankIdleInRank(int rank) const {
    for (int j = 0; j < config_.bankgroups; j++) {
        for (int k = 0; k < config_.banks_per_group; k++) {
            if (bank_states_[rank][j][k].IsRowOpen()) {
                return false;
            }
        }
    }
    return true;
}

bool ChannelState::IsRWPendingOnRef(const Command& cmd) const {
    int rank = cmd.Rank();
    int bankgroup = cmd.Bankgroup();
    int bank = cmd.Bank();
    return (IsRowOpen(rank, bankgroup, bank) &&
            RowHitCount(rank, bankgroup, bank) == 0 &&
            bank_states_[rank][bankgroup][bank].OpenRow() == cmd.Row());
}

void ChannelState::BankNeedRefresh(int rank, int bankgroup, int bank,
                                   bool need) {
    if (need) {
        Address addr = Address(-1, rank, bankgroup, bank, -1, -1);
        refresh_q_.emplace_back(CommandType::REFRESH_BANK, addr, -1);
    } else {
        for (auto it = refresh_q_.begin(); it != refresh_q_.end(); it++) {
            if (it->Rank() == rank && it->Bankgroup() == bankgroup &&
                it->Bank() == bank) {
                refresh_q_.erase(it);
                break;
            }
        }
    }
    return;
}

void ChannelState::RankNeedRefresh(int rank, bool need) {
    if (need) {
        Address addr = Address(-1, rank, -1, -1, -1, -1);
        refresh_q_.emplace_back(CommandType::REFRESH, addr, -1);
    } else {
        for (auto it = refresh_q_.begin(); it != refresh_q_.end(); it++) {
            if (it->Rank() == rank) {
                refresh_q_.erase(it);
                break;
            }
        }
    }
    return;
}

Command ChannelState::GetReadyCommand(const Command& cmd, uint64_t clk) const {
    Command ready_cmd = Command();
    if (cmd.IsRankCMD()) {
        int num_ready = 0;
        for (auto j = 0; j < config_.bankgroups; j++) {
            for (auto k = 0; k < config_.banks_per_group; k++) {
                ready_cmd =
                    bank_states_[cmd.Rank()][j][k].GetReadyCommand(cmd, clk);
                if (!ready_cmd.IsValid()) {  // Not ready
                    continue;
                }
                if (ready_cmd.cmd_type != cmd.cmd_type) {  // likely PRECHARGE
                    Address new_addr =
                        Address(-1, cmd.Rank(), j, k, ready_cmd.Row(), -1);
                    ready_cmd.addr = new_addr;
                    return ready_cmd;
                } else {
                    num_ready++;
                }
            }
        }
        // All bank ready
        if (num_ready == config_.banks) {
            return ready_cmd;
        } else {
            return Command();
        }
    } else {
        ready_cmd = bank_states_[cmd.Rank()][cmd.Bankgroup()][cmd.Bank()]
                        .GetReadyCommand(cmd, clk);
        if (!ready_cmd.IsValid()) {
            return Command();
        }
        if (ready_cmd.cmd_type == CommandType::ACTIVATE) {
            if (!ActivationWindowOk(ready_cmd.Rank(), clk)) {
                return Command();
            }
        }
        return ready_cmd;
    }
}

void ChannelState::UpdateState(const Command& cmd) {
    if (cmd.IsRankCMD()) {
        for (auto j = 0; j < config_.bankgroups; j++) {
            for (auto k = 0; k < config_.banks_per_group; k++) {
                bank_states_[cmd.Rank()][j][k].UpdateState(cmd);
            }
        }
        if (cmd.IsRefresh()) {
            RankNeedRefresh(cmd.Rank(), false);
        } else if (cmd.cmd_type == CommandType::SREF_ENTER) {
            rank_is_sref_[cmd.Rank()] = true;
        } else if (cmd.cmd_type == CommandType::SREF_EXIT) {
            rank_is_sref_[cmd.Rank()] = false;
        }
    } else {
        bank_states_[cmd.Rank()][cmd.Bankgroup()][cmd.Bank()].UpdateState(cmd);
        if (cmd.IsRefresh()) {
            BankNeedRefresh(cmd.Rank(), cmd.Bankgroup(), cmd.Bank(), false);
        }
    }
    return;
}

void ChannelState::UpdateTiming(const Command& cmd, uint64_t clk) {
    switch (cmd.cmd_type) {
        case CommandType::TOGGLE_ON:
        case CommandType::TOGGLE_OFF:
            // Toggle timing is strictly local to the addressed bank.
            // Unlike the static JEDEC timing table, these two latencies are
            // lifecycle-controlled at runtime.  Existing next-command
            // constraints remain intact; the live value applies to toggle
            // commands issued after the update.
            for (const auto& cmd_timing :
                 timing_.same_bank[static_cast<int>(cmd.cmd_type)]) {
                const int latency =
                    cmd.cmd_type == CommandType::TOGGLE_ON
                        ? config_.tTGON
                        : config_.tTGOFF;
                bank_states_[cmd.Rank()][cmd.Bankgroup()][cmd.Bank()]
                    .UpdateTiming(cmd_timing.first, clk + latency);
            }
            break;
        case CommandType::ACTIVATE:
            UpdateActivationTimes(cmd.Rank(), clk);
            [[fallthrough]];
        case CommandType::READ:
        case CommandType::READ_PRECHARGE:
        case CommandType::WRITE:
        case CommandType::WRITE_PRECHARGE:
        case CommandType::PRECHARGE:
        case CommandType::REFRESH_BANK:
            // TODO - simulator speed? - Speciazlize which of the below
            // functions to call depending on the command type  Same Bank
            if (is_pim_mode_) {
                UpdateSameBankTiming(
                    cmd.addr, timing_.pim_mode[static_cast<int>(cmd.cmd_type)],
                    clk);
            } else {
                UpdateSameBankTiming(
                    cmd.addr, timing_.same_bank[static_cast<int>(cmd.cmd_type)],
                    clk);
            }

            // Same Bankgroup other banks
            UpdateOtherBanksSameBankgroupTiming(
                cmd.addr,
                timing_
                    .other_banks_same_bankgroup[static_cast<int>(cmd.cmd_type)],
                clk);

            // Other bankgroups
            UpdateOtherBankgroupsSameRankTiming(
                cmd.addr,
                timing_
                    .other_bankgroups_same_rank[static_cast<int>(cmd.cmd_type)],
                clk);

            // Other ranks
            UpdateOtherRanksTiming(
                cmd.addr, timing_.other_ranks[static_cast<int>(cmd.cmd_type)],
                clk);
            break;
        case CommandType::REFRESH:
        case CommandType::SREF_ENTER:
        case CommandType::SREF_EXIT:
            UpdateSameRankTiming(
                cmd.addr, timing_.same_rank[static_cast<int>(cmd.cmd_type)],
                clk);
            break;
        case CommandType::SIZE:
            AbruptExit(__FILE__, __LINE__);
    }
    return;
}

void ChannelState::UpdateSameBankTiming(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto cmd_timing : cmd_timing_list) {
        bank_states_[addr.rank][addr.bankgroup][addr.bank].UpdateTiming(
            cmd_timing.first, clk + cmd_timing.second);
    }
    return;
}

void ChannelState::UpdateOtherBanksSameBankgroupTiming(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto k = 0; k < config_.banks_per_group; k++) {
        if (k != addr.bank) {
            for (auto cmd_timing : cmd_timing_list) {
                bank_states_[addr.rank][addr.bankgroup][k].UpdateTiming(
                    cmd_timing.first, clk + cmd_timing.second);
            }
        }
    }
    return;
}

void ChannelState::UpdateOtherBankgroupsSameRankTiming(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto j = 0; j < config_.bankgroups; j++) {
        if (j != addr.bankgroup) {
            for (auto k = 0; k < config_.banks_per_group; k++) {
                for (auto cmd_timing : cmd_timing_list) {
                    bank_states_[addr.rank][j][k].UpdateTiming(
                        cmd_timing.first, clk + cmd_timing.second);
                }
            }
        }
    }
    return;
}

void ChannelState::UpdateOtherRanksTiming(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto i = 0; i < config_.ranks; i++) {
        if (i != addr.rank) {
            for (auto j = 0; j < config_.bankgroups; j++) {
                for (auto k = 0; k < config_.banks_per_group; k++) {
                    for (auto cmd_timing : cmd_timing_list) {
                        bank_states_[i][j][k].UpdateTiming(
                            cmd_timing.first, clk + cmd_timing.second);
                    }
                }
            }
        }
    }
    return;
}

void ChannelState::UpdateSameRankTiming(
    const Address& addr,
    const std::vector<std::pair<CommandType, int>>& cmd_timing_list,
    uint64_t clk) {
    for (auto j = 0; j < config_.bankgroups; j++) {
        for (auto k = 0; k < config_.banks_per_group; k++) {
            for (auto cmd_timing : cmd_timing_list) {
                bank_states_[addr.rank][j][k].UpdateTiming(
                    cmd_timing.first, clk + cmd_timing.second);
            }
        }
    }
    return;
}

void ChannelState::UpdateTimingAndStates(const Command& cmd, uint64_t clk) {
    UpdateState(cmd);
    UpdateTiming(cmd, clk);
    return;
}

bool ChannelState::ActivationWindowOk(int rank, uint64_t curr_time) const {
    bool tfaw_ok = IsFAWReady(rank, curr_time);
    if (config_.IsGDDR()) {
        if (!tfaw_ok)
            return false;
        else
            return Is32AWReady(rank, curr_time);
    }
    return tfaw_ok;
}

void ChannelState::UpdateActivationTimes(int rank, uint64_t curr_time) {
    if (!four_aw_[rank].empty() && curr_time >= four_aw_[rank][0]) {
        four_aw_[rank].erase(four_aw_[rank].begin());
    }
    four_aw_[rank].push_back(curr_time + config_.tFAW);
    if (config_.IsGDDR()) {
        if (!thirty_two_aw_[rank].empty() &&
            curr_time >= thirty_two_aw_[rank][0]) {
            thirty_two_aw_[rank].erase(thirty_two_aw_[rank].begin());
        }
        thirty_two_aw_[rank].push_back(curr_time + config_.t32AW);
    }
    return;
}

bool ChannelState::IsFAWReady(int rank, uint64_t curr_time) const {
    if (!four_aw_[rank].empty()) {
        if (curr_time < four_aw_[rank][0] && four_aw_[rank].size() >= 4) {
            return false;
        }
    }
    return true;
}

bool ChannelState::Is32AWReady(int rank, uint64_t curr_time) const {
    if (!thirty_two_aw_[rank].empty()) {
        if (curr_time < thirty_two_aw_[rank][0] &&
            thirty_two_aw_[rank].size() >= 32) {
            return false;
        }
    }
    return true;
}

}  // namespace dramsim3
