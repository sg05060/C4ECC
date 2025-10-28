#include <cstring>
#include "BPC.h"

namespace comp
{

// BP Encoding Result Length
static const unsigned ZRL_CODE_SIZE[34] = {0, 3, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
static const unsigned allOneSize = 5;
static const unsigned zeroDBPSize = 5;

/* BP Encoding Table 
  1        -> uncompressed
  01       -> Z-RLE: 2~33
  001      -> Z-RLE: 1
  0001     -> All 1s
  00001    -> zero DBP
*/

double BPC::CompressLine(std::vector<uint8_t> &_dataLine)
{
  /*
    The original dataline is placed in row-wise order.
    And their DBPs are placed in col-wise order.
     ------------              -------------------
    |  data[0]   |            |   |   |   |   |   |
     ------------             | D | D | D | . | D |
    |  data[1]   |            | B | B | B | . | B |
     ------------      =>     | P | P | P | . | P |
    |    ...     |            | 0 | 1 | 2 | . | 3 |
     ------------             |   |   |   |   | 1 |
    |  data[31]  |            |   |   |   |   |   |
     ------------              -------------------
    */
  const unsigned lineSize = _dataLine.size();
  const unsigned uncompressedSize = BYTE * lineSize;

  // Converts uint8_t to uint64_t
  std::vector<int64_t> dataLine(lineSize / 4, 0); // data symbol size is 4 bytes 
  for (int i = 0; i < lineSize; i += 4) { 
    int64_t data;
    std::memcpy(&data, &_dataLine[i], 4);
    dataLine[i/4] = data;
  }
  
  // Step1: Delta
  std::vector<int64_t> deltas;  // delta symbol size is 32-bit + 1
  for (size_t row = 1; row < dataLine.size(); row++)
    deltas.push_back(dataLine[row] - dataLine[row - 1]);

  // Step2: Bit-plane 
  int8_t prevDBP;
  int8_t DBP[33];
  int8_t DBX[33];

  for (int col = 32; col >= 0; col--) {  

    // Get the bit from each delta symbol
    int8_t buf = 0;
    for (int row = deltas.size() - 1; row >= 0; row--) {
      buf <<= 1;
      buf |= ((deltas[row] >> col) & 1);
    }

    // Step3: XOR
    if (col == 32) { // first plane is base plane
      DBP[32] = buf;
      DBX[32] = buf;
      prevDBP = buf;
    } else {  // XOR with previous base plane 
      DBP[col] = buf;
      DBX[col] = buf ^ prevDBP; // XOR with previous base plane
      prevDBP = buf;            // update previous base plane
    }
  }

  // Encode Delta-base 32-bit word (Original Encoder)
  unsigned compressedSize = encodeFirst(dataLine[0]);
  
  // Encode DBX and DBP (BP Encoder)
  compressedSize += encodeDeltas(DBP, DBX);

  // Update statistics
  m_Stat->Update(uncompressedSize, compressedSize);
  return (double)uncompressedSize / (double)compressedSize;
}

unsigned BPC::encodeFirst(int64_t base) {
  if (base == 0)                      // {3'b000}                      
    return 3;                            
  else if (isSignExtended(base, 4))   // {3'b001, base[3:0]}
    return 3 + 4;                        
  else if (isSignExtended(base, 8))   // {3'b010, base[7:0]}
    return 3 + 8;                       
  else if (isSignExtended(base, 16))  // {3'b011, base[15:0]}
    return 3 + 16;                      
  else                                // {1'b1, Uncompressed[31:0]}
    return 1 + 32;
}

unsigned BPC::encodeDeltas(int8_t* DBP, int8_t* DBX) {
  BPCResult* m_stat = static_cast<BPCResult*>(m_Stat);

  unsigned length = 0;
  unsigned runLength = 0;

  // Encoding DBX and DBP 
  for (int i = 32; i >= 0; i--) {
    if (DBX[i] == 0) {  // Z-RLE: Zero Run-Length Encoding
      runLength++;      // DBX=0, increment runLength
    }
    else {
      // Non-zero DBX found, update total length
      if (runLength > 0) { 
        length += ZRL_CODE_SIZE[runLength]; // Z-RLE code (0, 3, 7, 7, 7, ...)
        if (runLength == 1) 
          m_stat->UpdatePattern(1, (int)BPCPattern::ZERO);
        else
          m_stat->UpdatePattern(runLength, (int)BPCPattern::ZRLE);
        runLength = 0; // reset runLength
      }
      
      // DBX!=0, DBP=0
      if (DBP[i] == 0) { 
        length += zeroDBPSize;
        m_stat->UpdatePattern(1, (int)BPCPattern::ZeroDBP);
      }
      // All 1s
      else if (DBX[i] == 0x7f) { // 0111_1111 (7-bit symbol)
        length += allOneSize;
        m_stat->UpdatePattern(1, (int)BPCPattern::AllOnes);
      }
      // Uncompressed, {1'b1, Uncompressed[6:0]}
      else {
        length += 8;
        m_stat->UpdatePattern(1, (int)BPCPattern::Uncomp);
      }
    }
  }
  // Final Z-RLE
  if (runLength > 0) {
    length += ZRL_CODE_SIZE[runLength];
    if (runLength == 1) 
      m_stat->UpdatePattern(1, (int)BPCPattern::ZERO);
    else
      m_stat->UpdatePattern(runLength, (int)BPCPattern::ZRLE);
  }
  return length;
}

// Check if the value is sign-extended
bool BPC::isSignExtended(uint64_t value, uint8_t bitSize)
{
  uint64_t max = (1ULL << (bitSize - 1)) - 1;     // bitSize: 4 -> ...0000111
  uint64_t min = ~max;                            // bitSize: 4 -> ...1111000
  return (value <= max) | (value >= min);
}

// Check if the value is zero-extended
bool BPC::isZeroExtended(uint64_t value, uint8_t bitSize)
{
  uint64_t max = (1ULL << (bitSize)) - 1;         // bitSize: 4 -> ...0001111
  return (value <= max);
}

}
