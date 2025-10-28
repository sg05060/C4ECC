#ifndef __CUSTOM_BPC_H__
#define __CUSTOM_BPC_H__

#include "Compressor.h"
#include <vector>
#include <cstdint>
#include <cstdio>
#include <array>

#define USE_EXP_COMP 1    // 0: not use, 1: use
#define USE_MANT_COMP 0   // 0: not use, 1: use
#define BPC_VERBOSE 0     // 1: exp, 3: all, 4: CR, 5: pattern

namespace comp
{

class CustomBPC : public Compressor
{
public:
  CustomBPC(unsigned lineSize) {
    m_lastCompressedBits = 0;
    exp_patternCounts.fill(0);
    mant_patternCounts.fill(0);
  }
  //virtual ~CustomBPC(); 
  virtual double CompressLine(std::vector<uint8_t>& dataLine);
  unsigned GetLastCompressedSizeBits() const { return m_lastCompressedBits; }
  
  void PrintExpPatternStats() const {
    static const char* labels[] = {"ZRLE", "TWOP1", "ALL1", "ONE1", "TWOC1", "UNCP"};
    printf("[Exponent] ");
    for (int i = 0; i < 2; ++i) {
      printf("%s=%zu ", labels[i], exp_patternCounts[i]);
    }
    printf("\n");
  }

  void PrintMantPatternStats() const {
    static const char* labels[] = {"ZRLE", "TWOP1", "ALL1", "ONE1", "TWOC1", "UNCP"};
    printf("[Mantissa] ");
    for (int i = 0; i < 6; ++i) {
      printf("%s=%zu ", labels[i], mant_patternCounts[i]);
    }
    printf("\n");
  }

private:
  size_t compressExpField(const std::vector<uint8_t>& field);
  size_t compressMantField(const std::vector<uint32_t>& field);
  size_t encodeFirst(uint8_t base, uint8_t bits);
  size_t encodeExpDeltas(uint64_t* DBP, uint64_t* DBX, uint8_t bitwidth, size_t mask_bit);
  size_t encodeMantDeltas(uint64_t* DBP, uint64_t* DBX, uint8_t bitwidth, size_t mask_bit);
  bool isConsecutive(uint64_t bits);

private:
  unsigned m_lastCompressedBits = 0;

  std::array<size_t, 2> exp_patternCounts;  // 0:ZRLE, 1:UNCP
  std::array<size_t, 6> mant_patternCounts; // 0:ZRLE, 1:TWOP1, 2:ALL1, 3:ONE1, 4:TWOC1, 5:UNCP

  enum PatternType {
    ZRLE  = 0,
    TWOP1 = 1,
    ALL1  = 2,
    ONE1  = 3,
    TWOC1 = 4,
    UNCP  = 5
  };

protected:
  void accumulateExpPattern(PatternType type) {
    exp_patternCounts[static_cast<size_t>(type)]++;
  }
  void accumulateMantPattern(PatternType type) {
    mant_patternCounts[static_cast<size_t>(type)]++;
  }
};

} // namespace comp

#endif  // __CUSTOM_BPC_H__
