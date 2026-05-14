#include "bridge.h"
#include "memory_system.h"
#include "ms_ffi.h"
#include <cstdint>
#include <memory>

dramsim3_ext::dramsim3_ext(
        rust::Str config_file,
        rust::Str output_dir){

    ev_buffer.reserve(10000);

    auto read_cb = [this](uint64_t addr) {
        this->ev_buffer.push_back(dramsim3_mem_event{
            this->current_cycle,
            addr,
            false
        });
    };

    auto write_cb = [this](uint64_t addr) {
        this->ev_buffer.push_back(dramsim3_mem_event{
            this->current_cycle,
            addr,
            true
        });
    };


    std::string cpp_config_file(config_file.data(), config_file.length());
    std::string cpp_output_dir(output_dir.data(), output_dir.length());

    unique_ms = std::make_unique<dramsim3::MemorySystem>(cpp_config_file, cpp_output_dir, read_cb, write_cb);
}

rust::Vec<dramsim3_mem_event> dramsim3_ext::take_events() {
    rust::Vec<dramsim3_mem_event> payload = std::move(ev_buffer);

    ev_buffer = rust::Vec<dramsim3_mem_event>();
    ev_buffer.reserve(10000);

    return payload;
}

void dramsim3_ext::ClockTick(){
    unique_ms->ClockTick();
    current_cycle = unique_ms->GetClock();
}

double dramsim3_ext::GetTCK() {
	return unique_ms->GetTCK();
}
int dramsim3_ext::GetBusBits() {
	return unique_ms->GetBusBits();
}
int dramsim3_ext::GetBurstLength() {
	return unique_ms->GetBurstLength();
}
int dramsim3_ext::GetQueueSize() {
	return unique_ms->GetQueueSize();
}
int dramsim3_ext::GetClock(){
	return unique_ms->GetClock();
}

bool dramsim3_ext::GetPimMode(){
    return unique_ms->GetPimMode();
}

void dramsim3_ext::SetPimMode(bool new_mode){
    unique_ms->SetPimMode(new_mode);
}


void dramsim3_ext::GetBytes(size_t start_addr, int64_t &data_index, size_t &start_byte){

    int64_t data_index_; 
    size_t start_byte_;

    unique_ms->GetBytes(start_addr, &data_index_, &start_byte_);

    data_index = data_index_;
    start_byte = start_byte_;
    return;
}

uint64_t dramsim3_ext::GetSpatialGlobalAddr(const local_addr_bulk &local_addr){
    uint64_t ch = local_addr.channel;
    uint64_t ra = local_addr.rank;
    uint64_t bg = local_addr.bank_group;
    uint64_t ba = local_addr.bank;
    uint64_t local_addr_ = local_addr.bank_local_addr;

    return unique_ms->GetSpatialGlobalAddr(ch, ra, bg, ba, local_addr_);
}

uint64_t dramsim3_ext::BankLocalToGlobalAddr(const local_addr_bulk &local_addr){
    uint64_t ch = local_addr.channel;
    uint64_t ra = local_addr.rank;
    uint64_t bg = local_addr.bank_group;
    uint64_t ba = local_addr.bank;
    uint64_t local_addr_ = local_addr.bank_local_addr;

    return unique_ms->GetSpatialGlobalAddr(ch, ra, bg, ba, local_addr_);
}

uint64_t dramsim3_ext::ExactLocalToGlobalAddr(const local_addr_bulk &local_addr){
    uint64_t ch = local_addr.channel;
    uint64_t ra = local_addr.rank;
    uint64_t bg = local_addr.bank_group;
    uint64_t ba = local_addr.bank;
    uint64_t ro = local_addr.row;
    uint64_t co = local_addr.column;

    return unique_ms->ExactLocalToGlobalAddr(ch, ra, bg, ba, ro, co);
}

local_addr_bulk dramsim3_ext::GlobalToLocalAddr(uint64_t global_addr){
    local_addr_bulk local_bulk;
    unique_ms->GlobalToLocalAddr(
            &local_bulk.channel,
            &local_bulk.rank,
            &local_bulk.bank_group,
            &local_bulk.bank,
            &local_bulk.bank_local_addr,
            global_addr
            );

    return local_bulk;
}

uint64_t dramsim3_ext::GetRanks(){
	return unique_ms->GetRanks();
}

uint64_t dramsim3_ext::GetBanksPerBG(){
	return unique_ms->GetBanksPerBG();
}

uint64_t dramsim3_ext::GetBankgroupsPerRank(){
	return unique_ms->GetBankgroupsPerRank();
}

uint64_t dramsim3_ext::GetChannels(){
	return unique_ms->GetChannels();
}

bool dramsim3_ext::WillAcceptTransaction(uint64_t hex_addr, bool is_write){
    return unique_ms->WillAcceptTransaction(hex_addr, is_write);
}

bool dramsim3_ext::AddTransaction(uint64_t hex_addr, bool is_write, bool is_pim){

    return unique_ms->AddTransaction(hex_addr, is_write, is_pim);

}

std::unique_ptr<dramsim3_ext> create_sim(rust::Str config_file, rust::Str output_dir){
    return std::make_unique<dramsim3_ext>(config_file, output_dir);
}
