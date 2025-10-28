#ifndef COMP_BDI_H_
#define COMP_BDI_H_
#include "Compressor.h"
#include <vector>
#include <cstdint>
//#include "CompResult.h"

namespace comp
{

enum class BDIState
{
  Zeros = 0,
  Repeat = 1,
  Base8Delta1 = 2,
  Base8Delta2 = 3,
  Base8Delta4 = 4,
  Base4Delta1 = 5,
  Base4Delta2 = 6,
  Base2Delta1 = 7,
  Uncompressed = 8
};

struct BDIResult : public CompResult
{
  /*** constructors ***/
  BDIResult(unsigned lineSize)
    : CompResult(lineSize), Counts(9, 0) {};

  virtual void Update(unsigned uncompSize, unsigned compSize, int selected)
  {
    CompResult::Update(uncompSize, compSize);
    Counts[selected]++;
  }

  /*** member variables ***/
  std::vector<uint64_t> Counts;
};

class BDI : public Compressor
{
public:
  /*** constructor ***/
  BDI(unsigned lineSize)
  {
    m_Stat = new BDIResult(lineSize);
    m_Stat->CompressorName = "Base-Delta Immediate";
  }

  virtual double CompressLine(std::vector<uint8_t> &dataLine);

private:
  bool isZeros(std::vector<uint8_t>& dataLine);
  bool isRepeated(std::vector<uint8_t>& dataLine, const unsigned granularity);
  unsigned checkBDI(std::vector<uint8_t>& dataLine, const unsigned baseSize, const unsigned deltaSize);
  uint64_t reduceSign(uint64_t x);
};

}
#endif