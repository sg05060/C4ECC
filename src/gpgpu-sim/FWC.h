#ifndef __FWC_H__
#define __FWC_H__

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <iostream>

#include "Compressor.h"
namespace comp
{

struct HuffmanCode {
    std::string code;
    uint32_t dataBits;
};

class FWC : public Compressor
{
public:
    FWC();

    virtual double CompressLine(std::vector<uint8_t>& dataLine);

private:
    std::unordered_map<std::string, HuffmanCode> huffmanTable;

    std::vector<uint32_t> get32BitWords(const std::vector<uint8_t>& block);
    std::string patternCheck(uint32_t word, int idx, const std::vector<uint32_t>& prevWords);

    // Pattern checks
    bool isZero(uint32_t w) const { return w == 0u; }
    bool isSameAsLeft(uint32_t w, uint32_t l) const { return w == l; }
    bool isUpper8Zero(uint32_t w) const { return (w >> 24) == 0u; }
    bool isLower8Zero(uint32_t w) const { return (w & 0xFFu) == 0u; }
    bool isUpper16Zero(uint32_t w) const { return (w >> 16) == 0u; }
    bool isLower16Zero(uint32_t w) const { return (w & 0xFFFFu) == 0u; }

    int deltaRequiredBits(uint32_t w, uint32_t r) const;

bool isDeltaWithin(uint32_t w, uint32_t r, int bits) const;
uint16_t extractSignExp9(uint32_t w) const;

bool isExponentDeltaLeft(uint32_t w, uint32_t l, int bits) const;

bool isExponentDeltaRight(uint32_t w, uint32_t l, int bits) const;

// ---- 엔트리 1:1 대응 매처 구현 ----
bool match_all_zero(uint32_t w) const;


bool match_same_as_left(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const;
bool match_upper8_zero(uint32_t w) const;
bool match_lower8_zero(uint32_t w) const ;
bool match_upper16_zero(uint32_t w) const;
bool match_lower16_zero(uint32_t w) const;

bool match_delta_within_16bit(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const;
bool match_signexp_3_from_left(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const;
bool match_signexp_3_from_right(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const;
bool match_signexp_4_from_left(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const;
bool match_signexp_4_from_right(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const;
bool match_signexp_2_from_left(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const;
bool match_signexp_2_from_right(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const;

};

} // namespace comp

#endif  // __FWC_H_