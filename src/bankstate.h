#ifndef __BANKSTATE_H
#define __BANKSTATE_H

#include <vector>
#include "common.h"

namespace dramsim3 {

class BankState {
   public:
    explicit BankState(bool enable_pim_switch = false);

    enum class State {
        CLOSED_ISOLATED,
        CONNECTING_FOR_ACT,
        OPEN_CONNECTED,
        OPEN_ISOLATED,
        CONNECTING_FOR_PRE,
        PRECHARGING_CONNECTED,
        SREF,
        PD,
        SIZE
    };
    Command GetReadyCommand(const Command& cmd, uint64_t clk) const;

    // Update the state of the bank resulting after the execution of the command
    void UpdateState(const Command& cmd);

    // Update the existing timing constraints for the command
    void UpdateTiming(const CommandType cmd_type, uint64_t time);

    // Select the CPU or PIM near-row context at a drained mode boundary.
    void SetPimMode(bool mode);
    bool IsNearRowStable() const {
        return state_ == State::CLOSED_ISOLATED ||
               state_ == State::OPEN_ISOLATED;
    }

    bool IsRowOpen() const {
        return state_ == State::OPEN_CONNECTED ||
               state_ == State::OPEN_ISOLATED ||
               state_ == State::CONNECTING_FOR_PRE ||
               state_ == State::PRECHARGING_CONNECTED;
    }
    int OpenRow() const { return open_row_; }
    int RowHitCount() const { return row_hit_count_; }

   private:
    struct NearRowContext {
        bool is_open = false;
        int open_row = -1;
        int row_hit_count = 0;
    };

    // Current state of the Bank
    // Apriori or instantaneously transitions on a command.
    State state_;

    // Earliest time when the particular Command can be executed in this bank
    std::vector<uint64_t> cmd_timing_;

    // Currently open row
    int open_row_;

    // consecutive accesses to one row
    int row_hit_count_;

    bool enable_pim_switch_;
    bool is_pim_mode_;
    NearRowContext cpu_context_;
    NearRowContext pim_context_;

    // Address and transaction metadata for a multi-command toggle sequence.
    Command sequence_command_;

    void SaveNearRowContext(NearRowContext& context) const;
    void LoadNearRowContext(const NearRowContext& context);
};

}  // namespace dramsim3
#endif
