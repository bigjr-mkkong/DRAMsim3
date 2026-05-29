#ifndef __MS_FFI_H__
#define __MS_FFI_H__

#include "memory_system.h"
#include "ext/rust_bridge/cxx.h"
// #include "ext/rust_bridge/cxx.h"
#include <cstdint>
#include <memory>

struct dramsim3_mem_event;
struct local_addr_bulk;

class dramsim3_ext{
public:
    dramsim3_ext(rust::Str config_file, rust::Str output_dir);
    rust::Vec<dramsim3_mem_event> take_events();
    void ClockTick();

    double GetTCK();
    int GetBusBits();
    int GetBurstLength();
    int GetQueueSize();
    int GetClock();

    bool GetPimMode();
    void SetPimMode(bool new_mode);
    void GetBytes(size_t start_addr, int64_t &data_index_, size_t &start_byte_);


    uint64_t GetSpatialGlobalAddr(const local_addr_bulk &local_addr);
    uint64_t BankLocalToGlobalAddr(const local_addr_bulk &local_addr);
    uint64_t ExactLocalToGlobalAddr(const local_addr_bulk &local_addr);
    local_addr_bulk GlobalToLocalAddr(uint64_t global_addr);

    uint64_t GetRanks();
    uint64_t GetBanksPerBG();
    uint64_t GetBankgroupsPerRank();
    uint64_t GetChannels();

    bool WillAcceptTransaction(uint64_t hex_addr, bool is_write);
    bool AddTransaction(uint64_t hex_addr, bool is_write, bool is_pim);

private:
    std::unique_ptr<dramsim3::MemorySystem> unique_ms;
    rust::Vec<dramsim3_mem_event> ev_buffer;
    uint64_t current_cycle;
};

std::unique_ptr<dramsim3_ext> create_sim(rust::Str config_file, rust::Str output_dir);

#endif
