#ifndef COMP_COMPRESSOR_H_
#define COMP_COMPRESSOR_H_
#include <ios>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

//#include <fmt/core.h>

#include "CompResult.h"

namespace comp
{
    class Compressor
    {
        public:
        /*** getters ***/
        std::string GetCompressorName()
        {
            return m_Stat->CompressorName;
        }

        /*** methods ***/
        //virtual ~Compressor() {}
        virtual double CompressLine(std::vector<uint8_t> &dataLine) = 0;
        virtual CompResult* GetResult() { return m_Stat; }

        protected:
        CompResult *m_Stat; 
    };
}
#endif