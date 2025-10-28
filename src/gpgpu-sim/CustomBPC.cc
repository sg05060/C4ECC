#include <vector>
#include <cstdint>
#include <cstring>
#include "CustomBPC.h"

// 8-bit base + 7-bit DBP 1개 + 7-bit DBX x 8개

namespace comp {

static const unsigned _ZRL_CODE_SIZE[27] = {
  0, 3, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5
};
static const unsigned singleOneSize = 11;
static const unsigned consecutiveDoubleOneSize = 11;
static const unsigned PointedDoubleOne = 17;
static const unsigned allOneSize = 5;
static const unsigned zeroDBPSize = 5;

/* Exponent / Mantissa Table
  1     | uncomp          | 8-bit | Flag(1-bit) + Uncomp(7-bit) 
  01    | Z-RLE 2~9       | 5-bit | Flag(2-bit) + (RunLengh - 2)(3-bit) 
  001   | Z-RLE 1         | 3-bit | Flag(3-bit)
*/

/* Mantissa4 Table
  1     | uncomp          | 64-bit | Flag(1-bit) + Uncomp(63-bit) 
  01    | Z-RLE 2~5       |  7-bit | Flag(2-bit) + (RunLengh - 2)(5-bit) 
  001   | Z-RLE 1         |  3-bit | Flag(3-bit)
  00000 | Single 1        | 11-bit | Flag(5-bit) + StartPos(6-bit)
  00001 | Consec two 1's  | 11-bit | Flag(5-bit) + StartPos(6-bit)
  00010 | Zero DBP        |  5-bit | Flag(5-bit)
  00001 | All 1's         |  5-bit | Flag(5-bit)
*/


// Extract exponent and mantissa[22:19] from 32-bit float
inline void extract_exp_man(const uint8_t* bytes, uint8_t& exp, uint32_t& mant) {
  uint32_t word;
  std::memcpy(&word, bytes, 4);
  exp = (word >> 23) & 0xFF;
  mant = word & 0x7FFFFF;
}

double CustomBPC::CompressLine(std::vector<uint8_t>& dataLine) {
  const size_t num_floats = dataLine.size() / 4;

  std::vector<uint8_t> exponents(num_floats);
  std::vector<uint32_t> mantissas(num_floats);

  for (size_t i = 0; i < num_floats; ++i) {
    extract_exp_man(&dataLine[i * 4], exponents[i], mantissas[i]);
  }

  size_t original_bits = num_floats * 32;
  size_t exp_bits = num_floats * 8UL;
  if (USE_EXP_COMP) {
    exp_bits = compressExpField(exponents);
  }
  size_t mant_bits = num_floats * 23UL;
  if (USE_MANT_COMP) {
    mant_bits = compressMantField(mantissas);
  }
  size_t compressed_bits = 8 + exp_bits + mant_bits;
  m_lastCompressedBits = compressed_bits;

  // (debug) Print each block result
  if (BPC_VERBOSE == 4 || BPC_VERBOSE == 5) {
    printf("[BPC] original=%zub, exp=%zub/%zub, mant=%zub/%zub, total=%zub, ratio=%.4f\n",
            original_bits,
            exp_bits, num_floats * 8UL,
            mant_bits, num_floats * 23UL,
            compressed_bits,
            static_cast<double>(original_bits) / compressed_bits);
  }
  
  // (debug) Print Pattern of Compression Process
  if (BPC_VERBOSE == 5) {
    if (USE_EXP_COMP) {
      PrintExpPatternStats();
    }
    if (USE_MANT_COMP) {
      PrintMantPatternStats();
    }
  }

  return static_cast<double>(original_bits) / compressed_bits;
}


size_t CustomBPC::compressExpField(const std::vector<uint8_t>& field) {
  const size_t num_vals = field.size();

  // (debug) Origin Value
  if (BPC_VERBOSE == 1 || BPC_VERBOSE == 3) {
    printf("Exponent Original values:\n");
    for (auto val : field) {
      printf("  %3u = 0b", val);
      for (int i = 7; i >= 0; --i) {
        printf("%d", (val >> i) & 1);
      }
      printf("\n");
    }
  }

  // Step 1: Compute 9-bit signed deltas
  std::vector<int16_t> deltas;
  deltas.reserve(num_vals - 1);
  for (size_t i = 1; i < num_vals; ++i) {
    int16_t delta = static_cast<int16_t>(static_cast<int16_t>(field[i]) - static_cast<int16_t>(field[i - 1]));
    deltas.push_back(delta);
  }

  // (debug) Delta Value
  if (BPC_VERBOSE == 1 || BPC_VERBOSE == 3) {
    printf("Exponent Deltas:\n");
    for (auto d : deltas) {
      printf("  0b");
      for (int i = 8; i >= 0; --i) {
        printf("%d", (static_cast<uint16_t>(d) >> i) & 1);
      }
      printf("\n");
    }
  }

  // Step 2: Generate 63-bit bitplanes from deltas (MSB to LSB)
  const int num_planes = 9;
  uint64_t DBP[num_planes] = {0};
  uint64_t DBX[num_planes - 1] = {0};

  for (int bit = num_planes - 1; bit >= 0; --bit) {
    uint64_t plane = 0;
    for (size_t i = 0; i < deltas.size(); ++i) {
      plane |= ((static_cast<uint16_t>(deltas[i]) >> bit) & 0x1ULL) << i;
    }
    DBP[bit] = plane;
  }

  // (debug) Bit-plane Value
  if (BPC_VERBOSE == 1 || BPC_VERBOSE == 3) {
    printf("Exponent DBP:\n");
    for (int i = num_planes - 1; i >= 0; --i) {
      printf("  DBP[%d] = 0b", i);
      for (int b = deltas.size() - 1; b >= 0; --b) {
        printf("%llu", (DBP[i] >> b) & 1ULL);
      }
      printf("\n");
    }
  }

  // Step 3: XOR encode DBX[i] = DBP[i] ^ DBP[i+1]
  for (int i = 0; i < num_planes - 1; ++i) {
    DBX[i] = DBP[i] ^ DBP[i + 1];
  }

  // (debug) Bit-plane XOR Value
  if (BPC_VERBOSE == 1 || BPC_VERBOSE == 3) {
    printf("Exponent DBX:\n");
    for (int i = num_planes - 2; i >= 0; --i) {
      printf("  DBX[%d] = 0b", i);
      for (int b = deltas.size() - 1; b >= 0; --b) {
        printf("%llu", (DBX[i] >> b) & 1ULL);
      }
      printf("\n");
    }
  }

  // Step 4: Encode base (first value)
  size_t compressedSize = encodeFirst(field[0], 8);  // always 8-bit field assumed here

  // Step 5: Encode DBX planes
  compressedSize += encodeExpDeltas(DBP, DBX, num_planes, 7);
  return compressedSize;
}

size_t CustomBPC::compressMantField(const std::vector<uint32_t>& field) {
  const size_t num_vals = field.size();

  if (BPC_VERBOSE == 2 || BPC_VERBOSE == 3) {
      printf("Mantissa Original values:\n");
    for (auto val : field) {
      printf("  %7u = 0b", val);
      for (int i = 22; i >= 0; --i) {
          printf("%d", (val >> i) & 1);
      }
    printf("\n");
    }
  }

  // Step 1: Compute 24-bit signed deltas
  std::vector<int32_t> deltas;
  deltas.reserve(num_vals - 1);
  for (size_t i = 1; i < num_vals; ++i) {
    int32_t delta = static_cast<int32_t>(field[i]) - static_cast<int32_t>(field[i - 1]);
    deltas.push_back(delta);
  }

  if (BPC_VERBOSE == 2 || BPC_VERBOSE == 3) {
    printf("Mantissa Deltas:\n");
    for (auto d : deltas) {
    printf("  0b");
    for (int i = 4; i >= 0; --i) {
      printf("%d", (static_cast<uint8_t>(d) >> i) & 1);
    }
    printf("\n");
    }
  }

  // Step 2: Generate 63-bit bitplanes from deltas (MSB to LSB)
  const int num_planes = 24;  // For 24-bit deltas
  uint64_t DBP[num_planes] = {0};
  uint64_t DBX[num_planes - 1] = {0};

  for (int bit = num_planes - 1; bit >= 0; --bit) {
    uint64_t plane = 0;
    for (size_t i = 0; i < deltas.size(); ++i) {
        plane |= ((static_cast<uint32_t>(deltas[i]) >> bit) & 1ULL) << i;
    }
    DBP[bit] = plane;
  }

  if (BPC_VERBOSE == 2 || BPC_VERBOSE == 3) {
      printf("Mantissa DBP:\n");
    for (int i = num_planes - 1; i >= 0; --i) {
      printf("  DBP[%d] = 0b", i);
      for (int b = deltas.size() - 1; b >= 0; --b) {
        printf("%llu", (DBP[i] >> b) & 1ULL);
      }
      printf("\n");
    }
  } 

  // Step 3: XOR encode DBX[i] = DBP[i] ^ DBP[i+1]
  for (int i = 0; i < num_planes - 1; ++i) {
    DBX[i] = DBP[i] ^ DBP[i + 1];
  }

  if (BPC_VERBOSE == 2 || BPC_VERBOSE == 3) {
    printf("Mantissa DBX:\n");
    for (int i = num_planes - 2; i >= 0; --i) {
      printf("  DBX[%d] = 0b", i);
      for (int b = deltas.size() - 1; b >= 0; --b) {
        printf("%llu", (DBX[i] >> b) & 1ULL);
      }
      printf("\n");
    }
  }

  // Step 4: Encode base (first value)
  size_t compressedSize = encodeFirst(field[0], 23);

  // Step 5: Encode DBX planes
  compressedSize += encodeMantDeltas(DBP, DBX, num_planes, 63);
  return compressedSize;
}

size_t CustomBPC::encodeFirst(uint8_t base, uint8_t bits) {
  if (base == 0)
    return 3;
  else if ((base >> (bits - 4)) == 0)
    return 3 + 4;
  else
    return bits;
}

size_t CustomBPC::encodeExpDeltas(uint64_t* DBP, uint64_t* DBX, uint8_t bitwidth, size_t mask_bit) {
  size_t length = 0;
  unsigned runLength = 0;
  int planeCount = static_cast<int>(bitwidth);
  uint64_t mask = (mask_bit >= 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << mask_bit) - 1);

  for (int i = planeCount - 1; i >= 0; --i) {
    // uint64_t dbp = DBP[i] & mask;
    uint64_t dbx = DBX[i] & mask;

    // Start ZRLE
    if (dbx == 0) {
      runLength++;
      accumulateExpPattern(ZRLE);
    } 
    // Stop ZRLE
    else {  
      // Pattern: ZRLE
      if (runLength > 0) {
        length += _ZRL_CODE_SIZE[runLength];
        runLength = 0;
      }
      else { // Pattern: Uncomp
          length += 8;
          accumulateExpPattern(UNCP);
      }
    }
  }
  
  // All ZRLE
  if (runLength > 0) {
    length += _ZRL_CODE_SIZE[runLength];
  }

  return length;
}

size_t CustomBPC::encodeMantDeltas(uint64_t* DBP, uint64_t* DBX, uint8_t bitwidth, size_t mask_bit) {
  size_t length = 0;
  unsigned runLength = 0;
  int planeCount = static_cast<int>(bitwidth);
  uint64_t mask = (mask_bit >= 64) ? 0xFFFFFFFFFFFFFFFFULL : ((1ULL << mask_bit) - 1);

  for (int i = planeCount - 1; i >= 0; --i) {
    // uint64_t dbp = DBP[i] & mask;
    uint64_t dbx = DBX[i] & mask;

    // Start ZRLE
    if (dbx == 0) {
      runLength++;
      accumulateMantPattern(ZRLE);
    } 
    // Stop ZRLE
    else {  
      // Pattern: ZRLE
      if (runLength > 0) {
        length += _ZRL_CODE_SIZE[runLength];
        runLength = 0;
      }
      // Pattern: All 1's
      else if (dbx == mask) {
        length += allOneSize;
      } 
      else {
        int oneCnt = __builtin_popcountll(dbx);
        // Pattern: Single 1
        if (oneCnt == 1) {
          length += singleOneSize;
          accumulateMantPattern(ONE1);
        } 
        // Pattern: Consec two 1's
        else if (oneCnt == 2 && isConsecutive(dbx)) {
          length += consecutiveDoubleOneSize;
          accumulateMantPattern(TWOC1);
        } 
        else if (oneCnt == 2) {
          length += PointedDoubleOne;
          accumulateMantPattern(TWOP1);
        } 
        // Pattern: Uncomp
        else {
          length += 64;
          accumulateMantPattern(UNCP);
        }
      }
    }
  }
  // All ZRLE
  if (runLength > 0) {
    length += _ZRL_CODE_SIZE[runLength];
  }

  return length;
}

  bool CustomBPC::isConsecutive(uint64_t bits) {
    int first = -1, second = -1;
    for (int i = 0; i < 63; ++i) { // bitplane has up to 63 bits
      if ((bits >> i) & 1ULL) {
        if (first == -1)
          first = i;
        else {
          second = i;
          break;
        }
      }
    }
    return (second - first) == 1;
  }
} // namespace comp
