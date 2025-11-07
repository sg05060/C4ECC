#include "FWC.h"
#include <cstring>
#include <cmath>
#include <vector>

namespace comp
{
FWC::FWC() {
    huffmanTable = {
        {"incompressible", {"001", 32}},                        // -> 35b
        {"sign+exponent 3-bit delta from left", {"000", 26}},   // 23 + 3 = 26  -> 29b 
        {"sign+exponent 3-bit delta from right", {"011", 26}},  // 23 + 3 = 26  -> 29b
        {"sign+exponent 2-bit delta from left", {"1010", 27}},  // 23 + 2 = 25  -> 29b
        {"sign+exponent 2-bit delta from right", {"1100", 27}}, // 23 + 2 = 25  -> 29b
        {"sign+exponent 4-bit delta from left", {"1100", 27}}, // 23 + 4 = 27  -> 31b
        {"sign+exponent 4-bit delta from right", {"1100", 27}}, // 23 + 4 = 27  -> 31b
        {"upper 8-bit zero", {"1001", 24}},     // -> 28b
        {"lower 8-bit zero", {"1011", 24}},     // -> 28b
        {"upper 16-bit zero", {"11111", 16}},    // -> 21b
        {"lower 16-bit zero", {"11010", 16}},   // -> 20b
        {"all zero", {"11011", 0}},             // -> 5b
        {"same as left", {"11100", 0}},         // -> 5b
        {"delta within 16-bit", {"11101", 16}}, // -> 21b
    };
}

double FWC::CompressLine(std::vector<uint8_t>& dataLine) {
    // 32B 라인을 32bit × 8개로 분해 (엔디안은 get32BitWords 내부에서 선택)
    std::vector<uint32_t> blockWords = get32BitWords(dataLine);

    std::vector<uint32_t> prevWords;
    prevWords.reserve(8);

    unsigned lineBits = 0;

    for (int idx = 0; idx < 8; ++idx) {
        // 기존 함수 서명/흐름 그대로: (word, idx, prevWords)
        std::string pattern = patternCheck(blockWords[idx], idx, prevWords);
        //printf("WORD[%d] : ", idx);
        //std::cout << pattern << std::endl;
        // 테이블 조회 (없으면 incompressible로 폴백)
        auto it = huffmanTable.find(pattern);
        const HuffmanCode& hc =
            (it != huffmanTable.end()) ? it->second : huffmanTable.at("incompressible");

        lineBits += static_cast<unsigned>(hc.code.size()) + hc.dataBits;

        prevWords.push_back(blockWords[idx]);
    }

    return double(256)/double(lineBits);
}

std::vector<uint32_t> FWC::get32BitWords(const std::vector<uint8_t>& block) {
    std::vector<uint32_t> words;
    words.reserve(8);
    for (int i = 0; i < 32; i += 4) {
        uint32_t word = 0;
        std::memcpy(&word, block.data() + i, 4);
        //words.push_back(be32toh(word));  // big-endian → host
        words.push_back(word);
    }
    return words;
}

int FWC::deltaRequiredBits(uint32_t w, uint32_t r) const {
    uint32_t d = w ^ r;
    if (d == 0u) return 0;
    return 32 - __builtin_clz(d);
}

bool FWC::isDeltaWithin(uint32_t w, uint32_t r, int bits) const {
    if (bits <= 0) return (w == r);
    return deltaRequiredBits(w, r) <= bits;
}

uint16_t FWC::extractSignExp9(uint32_t w) const {
    uint16_t sign = (w >> 31) & 1u;
    uint16_t exp = (w >> 23) & 0xFFu;
    return uint16_t((sign << 8) | exp);
}

//bool FWC::isExponentDeltaBias(uint32_t w, int bits) const {
//    const uint16_t se = extractSignExp9(w);
//    const uint16_t bias = uint16_t((0u << 8) | 127u);
//    if (se < bias) return false; // non-negative only
//    uint16_t d = uint16_t(se - bias);
//    return d < (1u << bits);
//}

bool FWC::isExponentDeltaLeft(uint32_t w, uint32_t l, int bits) const {
    const uint16_t seW = extractSignExp9(w);
    const uint16_t seL = extractSignExp9(l);
    if (seW < seL) return false;
    uint16_t d = uint16_t(seW - seL);
    return d < (1u << bits);
}

bool FWC::isExponentDeltaRight(uint32_t w, uint32_t l, int bits) const {
    const uint16_t seW = extractSignExp9(w);
    const uint16_t seL = extractSignExp9(l);
    if (seW > seL) return false;
    uint16_t d = uint16_t(seL - seW);
    return d < (1u << bits);
}

// ---- 엔트리 1:1 대응 매처 구현 ----
bool FWC::match_all_zero(uint32_t w) const { return isZero(w); }


bool FWC::match_same_as_left(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const {
if (idx == 0) return false; return isSameAsLeft(w, prev[idx-1]);
}

bool FWC::match_upper8_zero(uint32_t w) const { return isUpper8Zero(w); }
bool FWC::match_lower8_zero(uint32_t w) const { return isLower8Zero(w); }
bool FWC::match_upper16_zero(uint32_t w) const { return isUpper16Zero(w); }
bool FWC::match_lower16_zero(uint32_t w) const { return isLower16Zero(w); }

bool FWC::match_delta_within_16bit(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const {
    if (idx == 0) return false; return isDeltaWithin(w, prev[idx-1], 16);
}

bool FWC::match_signexp_3_from_left(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const {
    if (idx == 0) return false; return isExponentDeltaLeft(w, prev[idx-1], 3);
}
bool FWC::match_signexp_3_from_right(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const {
    if (idx == 0) return false; return isExponentDeltaRight(w, prev[idx-1], 3);
}


bool FWC::match_signexp_4_from_left(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const {
    if (idx == 0) return false; return isExponentDeltaLeft(w, prev[idx-1], 4);
}

bool FWC::match_signexp_4_from_right(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const {
    if (idx == 0) return false; return isExponentDeltaRight(w, prev[idx-1], 4);
}

bool FWC::match_signexp_2_from_left(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const {
    if (idx == 0) return false; return isExponentDeltaLeft(w, prev[idx-1], 2);
}

bool FWC::match_signexp_2_from_right(uint32_t w, const std::vector<uint32_t>& prev, size_t idx) const {
    if (idx == 0) return false; return isExponentDeltaRight(w, prev[idx-1], 2);
}

std::string FWC::patternCheck(uint32_t word, int idx, const std::vector<uint32_t>& prevWords) {
    if (match_all_zero(word)) return "all zero";
    if (match_same_as_left(word, prevWords, idx)) return "same as left";
    if (match_lower16_zero(word)) return "lower 16-bit zero";
    if (match_upper16_zero(word)) return "upper 16-bit zero";
    if (match_delta_within_16bit(word, prevWords, idx)) return "delta within 16-bit";
    if (match_lower8_zero(word)) return "lower 8-bit zero";
    if (match_upper8_zero(word)) return "upper 8-bit zero";
    if (match_signexp_3_from_left(word, prevWords, idx)) return "sign+exponent 3-bit delta from left";
    if (match_signexp_3_from_right(word, prevWords, idx)) return "sign+exponent 3-bit delta from right";
    if (match_signexp_2_from_left(word, prevWords, idx)) return "sign+exponent 2-bit delta from left";
    if (match_signexp_2_from_right(word, prevWords, idx)) return "sign+exponent 2-bit delta from right";
    if (match_signexp_4_from_left(word, prevWords, idx)) return "sign+exponent 4-bit delta from left";
    if (match_signexp_4_from_right(word, prevWords, idx)) return "sign+exponent 4-bit delta from right";
    return "incompressible";
}

} // namespace comp
