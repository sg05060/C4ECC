#ifndef COMP_COMPRESULT_H_
#define COMP_COMPRESULT_H_

#include <iostream>
#include <fstream>
#include <string>

//#include <fmt/core.h>

//#include "utils.h"


#define BYTE (8)
#define BYTEMAX  (0xff)
#define BYTE2MAX (0xffff)
#define BYTE4MAX (0xffffffff)
#define BYTE8MAX (0xffffffffffffffff)

#define COMPSIZELIMIT ((ACCESS_GRAN * BYTE) + 32)

namespace comp
{
    struct CompResult
    {
        CompResult(unsigned lineSize)
            : LineSize(lineSize),
            OriginalSize(0), CompressedSize(0), CompRatio(0) {};

        virtual void Update(unsigned uncompSize, unsigned compSize, int selected = 0)
        {
            OriginalSize += uncompSize;
            CompressedSize += compSize;
            CompRatio = (double)OriginalSize / (double)CompressedSize;
        }

        /*** member varibles ***/
        std::string CompressorName;
        const unsigned LineSize;

        uint64_t OriginalSize;
        uint64_t CompressedSize;
        double CompRatio;
    };
}

#endif
