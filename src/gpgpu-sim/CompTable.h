#ifndef COMPTABLE_INCLUDED
#define COMPTABLE_INCLUDED
#include <unordered_map>
#include <cstdint>

struct CompPageBits {
  uint64_t seen[2] = {0,0}; // 기록 여부(128bit)
  uint64_t val[2]  = {0,0}; // 성공 여부(128bit)
};

class CompInfoTable {
public:
  static const size_t kLineBytes = 32;
  static const size_t kPageBytes = 4096; // 임의(4KB). 필요시 config로.

  void clear() { pages_.clear(); }

  // 기록: is_comp == true(성공) / false(실패)
  inline void set(uint64_t addr, bool is_comp) {
    uint64_t pk = page_key(addr);
    unsigned idx = line_idx(addr);
    unsigned w = idx >> 6, b = idx & 63;
    CompPageBits &pg = pages_[pk];
    pg.seen[w] |= (1ull << b);
    if (is_comp) pg.val[w] |= (1ull << b);
    else         pg.val[w] &= ~(1ull << b);
  }

  // 조회: false면 “모름(기록 안됨)”
  inline bool get(uint64_t addr, bool &is_comp_out) {
    uint64_t pk = page_key(addr);
    auto it = pages_.find(pk);
    if (it == pages_.end()) return false;
    unsigned idx = line_idx(addr);
    unsigned w = idx >> 6, b = idx & 63;
    const CompPageBits &pg = it->second;
    if (((pg.seen[w] >> b) & 1ull) == 0ull) return false;
    is_comp_out = ((pg.val[w] >> b) & 1ull) != 0ull;
    return true;
  }

private:
  static inline uint64_t page_key(uint64_t a) {
    return a & ~(uint64_t(kPageBytes-1));
  }
  static inline unsigned line_idx(uint64_t a) {
    return unsigned((a & (kPageBytes-1)) / kLineBytes); // 0..127
  }

  std::unordered_map<uint64_t, CompPageBits> pages_;
};
#endif