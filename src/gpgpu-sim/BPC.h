#ifndef __BPC_H__
#define __BPC_H__

#include "Compressor.h"
#include <string>
#include <sstream>
#include <vector>

#define NUM_BPC_PATTERN 5

namespace comp
{

enum class BPCPattern {
  Uncomp          = 0,
  ZRLE            = 1,
  ZERO            = 2,
  ZeroDBP         = 3,
  AllOnes         = 4,
};

struct BPCResult : public CompResult
{
  /*** constructors ***/
  BPCResult(unsigned lineSize)
    : CompResult(lineSize), Counts(NUM_BPC_PATTERN, 0), TotalWords(0) {};

  void UpdatePattern(unsigned numWords, int selected) {
    TotalWords += numWords;
    Counts[selected] += numWords;
  }

  void PrintPattern(std::stringstream& OFStream_buf) const {
    
    // Pattern names
    const std::vector<std::string> patternNames = {
      "Uncomp", "ZRLE", "ZERO", "ZeroDBP", "AllOnes"
    };

      // Add pattern counts
      OFStream_buf << "BPC Pattern Counts\n";
      for (size_t i = 0; i < NUM_BPC_PATTERN; i++) {
          OFStream_buf << patternNames[i] << ": " << Counts[i] << '\n';
      }

      // Add total words
      OFStream_buf << "Total Words: " << TotalWords << '\n';
  }

  /*** member variables ***/
  std::vector<uint64_t> Counts;
  uint64_t TotalWords;
};

class BPC : public Compressor
{
public:
  /*** constructor ***/
  BPC(unsigned lineSize)
  {
    m_Stat = new BPCResult(lineSize);
    m_Stat->CompressorName = "Bit-Plane Compression";
  }

  virtual double CompressLine(std::vector<uint8_t> &dataLine);

private:
  unsigned encodeFirst(int64_t base);
  unsigned encodeDeltas(int8_t* DBP, int8_t* DBX);
  bool isSignExtended(uint64_t value, uint8_t bitSize);
  bool isZeroExtended(uint64_t value, uint8_t bitSize);
};

}

#endif  // __BPC_H__
