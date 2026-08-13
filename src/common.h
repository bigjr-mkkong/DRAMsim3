#ifndef __COMMON_H
#define __COMMON_H

#include <cstdint>
#include <stdint.h>
#include <iostream>
#include <vector>
#include <map>

namespace dramsim3 {

struct Address {
    Address()
        : channel(-1), rank(-1), bankgroup(-1), bank(-1), row(-1), column(-1) {}
    Address(int channel, int rank, int bankgroup, int bank, int row, int column)
        : channel(channel),
          rank(rank),
          bankgroup(bankgroup),
          bank(bank),
          row(row),
          column(column) {}
    Address(const Address& addr)
        : channel(addr.channel),
          rank(addr.rank),
          bankgroup(addr.bankgroup),
          bank(addr.bank),
          row(addr.row),
          column(addr.column) {}
    int channel;
    int rank;
    int bankgroup;
    int bank;
    int row;
    int column;
};

inline uint32_t ModuloWidth(uint64_t addr, uint32_t bit_width, uint32_t pos) {
    addr >>= pos;
    auto store = addr;
    addr >>= bit_width;
    addr <<= bit_width;
    return static_cast<uint32_t>(store ^ addr);
}

// extern std::function<Address(uint64_t)> AddressMapping;
int GetBitInPos(uint64_t bits, int pos);
// it's 2017 and c++ std::string still lacks a split function, oh well
std::vector<std::string> StringSplit(const std::string& s, char delim);
template <typename Out>
void StringSplit(const std::string& s, char delim, Out result);

int LogBase2(int power_of_two);
void AbruptExit(const std::string& file, int line);
bool DirExist(std::string dir);

enum class CommandType {
    READ,
    READ_PRECHARGE,
    WRITE,
    WRITE_PRECHARGE,
    ACTIVATE,
    PRECHARGE,
    REFRESH_BANK,
    REFRESH,
    SREF_ENTER,
    SREF_EXIT,
    TOGGLE_ON,
    TOGGLE_OFF,
    SIZE
};

struct Command {
    Command()
        : cmd_type(CommandType::SIZE), hex_addr(0), is_pim(false),
          transaction_id(0) {}
    Command(CommandType cmd_type, const Address& addr, uint64_t hex_addr)
        : cmd_type(cmd_type), addr(addr), hex_addr(hex_addr), is_pim(false),
          transaction_id(0) {}
    Command(CommandType cmd_type, const Address& addr, uint64_t hex_addr, bool is_pim)
        : cmd_type(cmd_type), addr(addr), hex_addr(hex_addr), is_pim(is_pim),
          transaction_id(0) {}
    Command(CommandType cmd_type, const Address& addr, uint64_t hex_addr,
            bool is_pim, uint64_t transaction_id)
        : cmd_type(cmd_type), addr(addr), hex_addr(hex_addr), is_pim(is_pim),
          transaction_id(transaction_id) {}
    // Command(const Command& cmd) {}

    bool IsValid() const { return cmd_type != CommandType::SIZE; }
    bool IsRefresh() const {
        return cmd_type == CommandType::REFRESH ||
               cmd_type == CommandType::REFRESH_BANK;
    }
    bool IsRead() const {
        return cmd_type == CommandType::READ ||
               cmd_type == CommandType ::READ_PRECHARGE;
    }
    bool IsWrite() const {
        return cmd_type == CommandType ::WRITE ||
               cmd_type == CommandType ::WRITE_PRECHARGE;
    }
    bool IsReadWrite() const { return IsRead() || IsWrite(); }
    bool IsToggle() const {
        return cmd_type == CommandType::TOGGLE_ON ||
               cmd_type == CommandType::TOGGLE_OFF;
    }
    bool IsRankCMD() const {
        return cmd_type == CommandType::REFRESH ||
               cmd_type == CommandType::SREF_ENTER ||
               cmd_type == CommandType::SREF_EXIT;
    }
    CommandType cmd_type;
    Address addr;
    uint64_t hex_addr;
    bool is_pim;
    // Controller-local identity used only to track promoted work across a
    // pause barrier. It does not cross the public DRAMSim3 callback ABI.
    uint64_t transaction_id;

    int Channel() const { return addr.channel; }
    int Rank() const { return addr.rank; }
    int Bankgroup() const { return addr.bankgroup; }
    int Bank() const { return addr.bank; }
    int Row() const { return addr.row; }
    int Column() const { return addr.column; }

    friend std::ostream& operator<<(std::ostream& os, const Command& cmd);
};

struct Transaction {
    Transaction() : id(0) {}
    Transaction(uint64_t addr, bool is_write)
        : id(0),
          addr(addr),
          added_cycle(0),
          complete_cycle(0),
          is_write(is_write),
          is_pim(false) {}
    Transaction(uint64_t addr, bool is_write, bool is_pim)
        : id(0),
          addr(addr),
          added_cycle(0),
          complete_cycle(0),
          is_write(is_write),
          is_pim(is_pim) {}
    Transaction(const Transaction& tran)
        : id(tran.id),
          addr(tran.addr),
          added_cycle(tran.added_cycle),
          complete_cycle(tran.complete_cycle),
          is_write(tran.is_write),
          is_pim(tran.is_pim) {}
    uint64_t id;
    uint64_t addr;
    uint64_t added_cycle;
    uint64_t complete_cycle;
    bool is_write;
    bool is_pim;

    friend std::ostream& operator<<(std::ostream& os, const Transaction& trans);
    friend std::istream& operator>>(std::istream& is, Transaction& trans);
};

template<typename K, typename V>
class IntervalMap {
  V val_begin_;
public:
  std::map<K, V> map_;

  IntervalMap(V const& val) : val_begin_(val) {}
  ~IntervalMap() {}

  void Assign(K const& keyBegin, K const& keyEnd, V const& val) {
    if (keyEnd <= keyBegin) {
      return;
    }

    V valEnd = operator[](keyEnd);

    auto iter_begin = (map_.insert_or_assign(keyBegin, val)).first;
    auto iter_end = (map_.insert_or_assign(keyEnd, valEnd)).first;

    iter_begin++;

    iter_end = map_.erase(iter_begin, iter_end);
    iter_end--;

    for (int i = 0; i < 2; i++) {
      if (iter_end != map_.begin()
          && std::prev(iter_end, 1) -> second == iter_end -> second) {
        iter_end = map_.erase(iter_end);
      } else if (iter_end == map_.begin() && iter_end -> second == val_begin_) {
        iter_end = map_.erase(iter_end);
      } else if (iter_end != map_.end()) {
        iter_end++;
      }
    }
  }

  void RemoveSurroundingInterval(K const& keyBegin, K const& keyEnd) {
    if (keyEnd < keyBegin) {
      return;
    }

    auto iter_begin = map_.lower_bound(keyBegin);
    auto iter_end = map_.upper_bound(keyEnd);

    iter_end = map_.erase(iter_begin, iter_end);
  }

  V const& operator[](K const& key) const {
    auto it = map_.upper_bound(key);
    if (it == map_.begin()) {
      return val_begin_;
    } else {
      return (--it)->second;
    }
  }

  K const& GetBaseAddr(K const& key) const {
    auto it = map_.upper_bound(key);
    if (it == map_.begin()) {
      return 0u;
    } else {
      return (--it)->first;
    }
  }
};

inline uint64_t count_ones(uint64_t n) {
  uint64_t count = 0;
  while (n > 0) {
    count = n % 2 == 1 ? count + 1 : count;
    n >>= 1;
  }
  return count;
}

}  // namespace dramsim3
#endif
