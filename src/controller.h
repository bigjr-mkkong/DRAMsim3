#ifndef __CONTROLLER_H
#define __CONTROLLER_H

#include <fstream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "channel_state.h"
#include "command_queue.h"
#include "common.h"
#include "refresh.h"
#include "simple_stats.h"

#ifdef THERMAL
#include "thermal.h"
#endif  // THERMAL

namespace dramsim3 {

enum class RowBufPolicy { OPEN_PAGE, CLOSE_PAGE, SIZE };

class Controller {
   public:
#ifdef THERMAL
    Controller(int channel, const Config &config, const Timing &timing,
               ThermalCalculator &thermalcalc);
#else
    Controller(int channel, const Config &config, const Timing &timing);
#endif  // THERMAL
    void ClockTick();
    void SetPimMode(bool mode);
    void RequestPause();
    bool IsPauseRequested() const { return pause_requested_; }
    bool IsPauseReady() const;
    void CommitPausedMode(bool mode);
    void CancelPause();
    uint64_t GetPauseParkedTransactions() const {
        return pause_parked_transactions_;
    }
    uint64_t GetPausePromotedTransactions() const {
        return pause_promoted_transactions_;
    }
    bool WillAcceptTransaction(uint64_t hex_addr, bool is_write) const;
    bool AddTransaction(Transaction trans);
    bool IsDrained() const;
    int QueueUsage() const;
    // Stats output
    void PrintEpochStats();
    void PrintFinalStats();
    void ResetStats() { simple_stats_.Reset(); }
    std::pair<uint64_t, int> ReturnDoneTrans(uint64_t clock);

    int channel_id_;

   private:
    uint64_t clk_;
    const Config &config_;
    SimpleStats simple_stats_;
    ChannelState channel_state_;
    CommandQueue cmd_queue_;
    Refresh refresh_;

#ifdef THERMAL
    ThermalCalculator &thermal_calc_;
#endif  // THERMAL

    // specify whether transactions should be executed from PIM side or CPU side
    bool is_pim_mode_ = false;
    // queue that takes transactions from CPU side
    bool is_unified_queue_;
    std::vector<Transaction> unified_queue_;
    std::vector<Transaction> read_queue_;
    std::vector<Transaction> pim_queue_;
    std::vector<Transaction> write_buffer_;

    // transactions that are not completed, use map for convenience
    std::multimap<uint64_t, Transaction> pending_pim_q_;
    std::multimap<uint64_t, Transaction> pending_rd_q_;
    std::multimap<uint64_t, Transaction> pending_wr_q_;

    // completed transactions
    std::vector<Transaction> return_queue_;

    // row buffer policy
    RowBufPolicy row_buf_policy_;

#ifdef CMD_TRACE
    std::ofstream cmd_trace_;
#endif  // CMD_TRACE

    // used to calculate inter-arrival latency
    uint64_t last_trans_clk_;

    // transaction queueing
    int write_draining_;
    struct TransactionProgress {
        bool promoted = false;
        bool command_complete = false;
        bool architectural_complete = false;
    };
    uint64_t next_transaction_id_ = 1;
    std::unordered_map<uint64_t, TransactionProgress> transaction_progress_;
    bool pause_requested_ = false;
    std::unordered_set<uint64_t> pause_pending_ids_;
    uint64_t pause_parked_transactions_ = 0;
    uint64_t pause_promoted_transactions_ = 0;

    void ScheduleTransaction();
    void IssueCommand(const Command &tmp_cmd);
    Command TransToCommand(const Transaction &trans);
    void UpdateCommandStats(const Command &cmd);
    void MarkPromoted(const Transaction &trans);
    void MarkReadGroupPromoted(uint64_t addr);
    void MarkCommandComplete(uint64_t transaction_id);
    void MarkArchitecturalComplete(uint64_t transaction_id);
    void RetireProgressIfComplete(uint64_t transaction_id);
    uint64_t ParkedTransactionCount() const;
};
}  // namespace dramsim3
#endif
