/*
* mt4-stooq-api - stooq.com C++ API
*
* Copyright (c) 2018 Elektro Yar. Email: git.electroyar@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include <iostream>
#include <iomanip>
#include <cctype>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include "mt4-stooq.hpp"

using json = nlohmann::json;

#define PROGRAM_VERSION "1.6"
#define PROGRAM_DATE "15.04.2020"

int main(int argc, char* argv[]) {
    std::cout << "forexprostools downloader" << std::endl;
    std::cout
        << "version: " << PROGRAM_VERSION
        << " date: " << PROGRAM_DATE
        << std::endl << std::endl;
    StooqApi stooq;

    //std::vector<xquotes_common::Candle> candles;
    stooq.get_historical_data("EURUSD", StooqApi::PeriodTypes::DAY, xtime::get_timestamp(1,1,2018),  xtime::get_timestamp(1,1,2020));

    return EXIT_FAILURE;
}

