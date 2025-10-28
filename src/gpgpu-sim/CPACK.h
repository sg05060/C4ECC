#ifndef COMP_CPACK_H_
#define COMP_CPACK_H_
#include <deque>
#include <vector>
#include <cstdint>
#include "Compressor.h"
//#include "CompResult.h"

#define WORDSIZE 4
#define DICTSIZE 64
#define NUM_ENTRY DICTSIZE/WORDSIZE

#define NUM_CPACK_PATTERN 6

namespace comp
{

enum class CPACKPattern
{
  ZZZZ = 0,
  ZZZX = 1,
  MMMM = 2,
  MMMX = 3,
  MMXX = 4,
  XXXX = 5,
};

struct CPACKResult : public CompResult
{
  /*** constructors ***/
  CPACKResult(unsigned lineSize)
    : CompResult(lineSize), TotalWords(0), Counts(NUM_CPACK_PATTERN, 0) {};

//  virtual void Update(unsigned uncompSize, unsigned compSize)
//  {
//    CompResult::Update(uncompSize, compSize);
//  }

  void UpdatePattern(int selected)
  {
    TotalWords++;
    Counts[selected]++;
  }

  /*** member variables ***/
  std::vector<uint64_t> Counts;
  uint64_t TotalWords;
};

class CPACK : public Compressor
{
public:
  CPACK(unsigned lineSize)
  {
    m_Stat = new CPACKResult(lineSize);
    m_Stat->CompressorName = "C-Pack";

    // init dictionary
    for (int i = 0; i < NUM_ENTRY; i++)
    {
      uint8_t *init = new uint8_t[WORDSIZE];
      for (int j = 0; j < WORDSIZE; j++)
        init[j] = 0;
      m_Dictionary.push_back(init);
    }
  }

  virtual double CompressLine(std::vector<uint8_t> &dataLine); 

private:
  std::deque<uint8_t*> m_Dictionary;

  // 0. zzzz (00)         : 2
  // 1. xxxx (01)BBBB     : 34
  // 2. mmmm (10)bbbb     : 6
  // 3. mmxx (1100)bbbbBB : 24
  // 4. zzzx (1100)B      : 12
  // 5. mmmx (1110)bbbbB  : 16
  const unsigned m_PatternLength[NUM_CPACK_PATTERN] = { 2, 34, 6, 24, 12, 16 };
};

}
#endif