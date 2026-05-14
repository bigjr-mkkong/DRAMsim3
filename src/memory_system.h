#ifndef __MEMORY_SYSTEM__H
#define __MEMORY_SYSTEM__H

#include <common.h>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "configuration.h"
#include "dram_system.h"
#include "hmc.h"

namespace dramsim3 {

// This should be the interface class that deals with CPU
class MemorySystem {
   public:
    MemorySystem(const std::string &config_file, const std::string &output_dir,
                 std::function<void(uint64_t)> read_callback,
                 std::function<void(uint64_t)> write_callback);
    ~MemorySystem();
    void ClockTick();
    void RegisterCallbacks(std::function<void(uint64_t)> read_callback,
                           std::function<void(uint64_t)> write_callback);
    double GetTCK() const;
    int GetBusBits() const;
    int GetBurstLength() const;
    int GetQueueSize() const;
    int GetClock() const { return dram_system_->GetClock(); };
    bool GetPimMode() const { return dram_system_->GetPimMode(); };
    void SetPimMode(bool mode) { dram_system_->SetPimMode(mode); };

    //TODO: Figure out what are these mmap
    void MMap(int64_t data_index, size_t start_addr, size_t end_addr, size_t offset);
    void MUnmap(size_t start_addr, size_t end_addr);

    void GetBytes(size_t start_addr, int64_t *data_index, size_t *start_byte);

    //TODO: Figure out how to pass rust String into cpp std::String
    int GetConfigParameter(std::string identifier);


    uint64_t GetSpatialGlobalAddr(uint64_t channel, uint64_t rank, uint64_t bankgroup, uint64_t bank, uint64_t hex_addr);
    uint64_t BankLocalToGlobalAddr(uint64_t channel, uint64_t rank, uint64_t bankgroup, uint64_t bank, uint64_t hex_addr);
    uint64_t ExactLocalToGlobalAddr(uint64_t channel, uint64_t rank, uint64_t bankgroup, uint64_t bank, uint64_t ro, uint64_t co);
    void GlobalToLocalAddr(uint64_t* channel, uint64_t* rank, uint64_t* bankgroup, uint64_t* bank, uint64_t* local_addr, uint64_t hex_addr);


    uint64_t GetRanks() const { return config_->ranks; }
    uint64_t GetBanksPerBG() const { return config_->banks_per_group; }
    uint64_t GetBankgroupsPerRank() const { return config_->bankgroups; }
    uint64_t GetChannels() const { return config_->channels; }
    void PrintStats() const;
    void ResetStats();

    bool WillAcceptTransaction(uint64_t hex_addr, bool is_write) const;
    bool AddTransaction(uint64_t hex_addr, bool is_write, bool is_pim = false);

   private:
    // These have to be pointers because Gem5 will try to push this object
    // into container which will invoke a copy constructor, using pointers
    // here is safe
    Config *config_;
    BaseDRAMSystem *dram_system_;
    IntervalMap<size_t, std::pair<int64_t, int64_t>> *mmap_;
};

MemorySystem* GetMemorySystem(const std::string &config_file, const std::string &output_dir,
                 std::function<void(uint64_t)> read_callback,
                 std::function<void(uint64_t)> write_callback);

}  // namespace dramsim3

#endif
