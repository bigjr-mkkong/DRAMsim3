#include "cpu.h"
#include "memory_system.h"
#include <sstream>

namespace dramsim3 {

void RandomCPU::ClockTick() {
    // Create random CPU requests at full speed
    // this is useful to exploit the parallelism of a DRAM protocol
    // and is also immune to address mapping and scheduling policies
    memory_system_.ClockTick();
    if (get_next_) {
        last_addr_ = gen();
        last_write_ = (gen() % 3 == 0);
    }
    
    if(clk_ == 10) {
        memory_system_.AddTransaction(PIM_START_ADDR, false);
    } else if(clk_ == 30) {
        memory_system_.AddTransaction(PIM_RESUME_ADDR, false);
    } else if(clk_ == 40) {
        memory_system_.AddTransaction(PIM_PAUSE_ADDR, false);
    } else if(clk_ == 50) {
        memory_system_.AddTransaction(PIM_RESUME_ADDR, false);
    } else if(clk_ == 70) {
        memory_system_.AddTransaction(PIM_QUERY_ADDR, false);
    } else {
        get_next_ = memory_system_.WillAcceptTransaction(last_addr_, last_write_);
        if (get_next_) {
            memory_system_.AddTransaction(last_addr_, last_write_);
        }
    }
    clk_++;
    return;
}

void StreamCPU::ClockTick() {
    // stream-add, read 2 arrays, add them up to the third array
    // this is a very simple approximate but should be able to produce
    // enough buffer hits

    // moving on to next set of arrays
    memory_system_.ClockTick();
    if (offset_ >= array_size_ || clk_ == 0) {
        addr_a_ = gen();
        addr_b_ = gen();
        addr_c_ = gen();
        offset_ = 0;
    }

    if (!inserted_a_ &&
        memory_system_.WillAcceptTransaction(addr_a_ + offset_, false)) {
        memory_system_.AddTransaction(addr_a_ + offset_, false);
        inserted_a_ = true;
    }
    if (!inserted_b_ &&
        memory_system_.WillAcceptTransaction(addr_b_ + offset_, false)) {
        memory_system_.AddTransaction(addr_b_ + offset_, false);
        inserted_b_ = true;
    }
    if (!inserted_c_ &&
        memory_system_.WillAcceptTransaction(addr_c_ + offset_, true)) {
        memory_system_.AddTransaction(addr_c_ + offset_, true);
        inserted_c_ = true;
    }
    // moving on to next element
    if (inserted_a_ && inserted_b_ && inserted_c_) {
        offset_ += stride_;
        inserted_a_ = false;
        inserted_b_ = false;
        inserted_c_ = false;
    }
    clk_++;
    return;
}

TraceBasedCPU::TraceBasedCPU(const std::string& config_file,
                             const std::string& output_dir,
                             const std::string& trace_file)
    : CPU(config_file, output_dir) {
    trace_file_.open(trace_file);
    if (trace_file_.fail()) {
        std::cerr << "Trace file does not exist" << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
}

void TraceBasedCPU::ClockTick() {
    memory_system_.ClockTick();
    if (!trace_file_.eof()) {
        if (get_next_) {
            get_next_ = false;
            trace_file_ >> trans_;
        }
        if (trans_.added_cycle <= clk_) {
            get_next_ = memory_system_.WillAcceptTransaction(trans_.addr,
                                                             trans_.is_write);
            if (get_next_) {
                memory_system_.AddTransaction(trans_.addr, trans_.is_write);
            }
        }
    }
    clk_++;
    return;
}

PRTraceCPU::PRTraceCPU(const std::string& config_file,
                             const std::string& output_dir,
                             const std::string& trace_file)
    : CPU(config_file, output_dir) {
    trace_file_.open(trace_file);
    if (trace_file_.fail()) {
        std::cerr << "PR Trace file does not exist" << std::endl;
        AbruptExit(__FILE__, __LINE__);
    }
}


void PRTraceCPU::ClockTick() {
    // Tick the underlying memory system
    memory_system_.ClockTick();

    // 1. Read the next transaction from our cleaned trace file
    if (!has_pending_tx_ && !trace_file_.eof()) {
        std::string line;
        if (std::getline(trace_file_, line)) {
            if (line.empty()) return;

            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;
            
            while (iss >> token) {
                tokens.push_back(token);
            }

            if (tokens.size() >= 2) {
                // The timestamp is always the last token
                target_tick_ = std::stoull(tokens.back());
                
                // The command or address is always the first token
                std::string cmd_or_addr = tokens[0];
                
                // If there's an operation (READ/WRITE), it's the middle token
                std::string op_str = (tokens.size() >= 3) ? tokens[1] : "";

                // Default memory operation handling
                pending_is_write_ = (op_str == "WRITE");

                // 2. Intercept Special Instructions
                if (cmd_or_addr == "PIM_START") {
                    pending_addr_ = PIM_START_ADDR;
                    pending_is_write_ = false; 
                } 
                else if (cmd_or_addr == "MEM_PAUSE") {
                    pending_addr_ = PIM_RESUME_ADDR;
                    pending_is_write_ = false;
                } 
                else if (cmd_or_addr == "MEM_RESUME") {
                    pending_addr_ = PIM_PAUSE_ADDR;
                    pending_is_write_ = false;
                } 
                else if (cmd_or_addr == "PIM_QUERY") {
                    pending_addr_ = PIM_QUERY_ADDR;
                    pending_is_write_ = false;
                } 
                else {
                    // Standard memory access: Parse the hex address natively
                    pending_addr_ = std::stoull(cmd_or_addr, nullptr, 16);
                }

                has_pending_tx_ = true;
            }
        }
    }

    // 3. Issue the transaction when the MEM local tick reaches the target
    if (has_pending_tx_) {
        if (clk_ >= target_tick_) {
            if (memory_system_.WillAcceptTransaction(pending_addr_, pending_is_write_)) {
                memory_system_.AddTransaction(pending_addr_, pending_is_write_);
                has_pending_tx_ = false; // Transaction consumed, ready for next line
            }
        }
    }
    // Advance the MEM local tick
    clk_++;
}
}  // namespace dramsim3
