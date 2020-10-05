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
#include "mt4-csv.hpp"
#include "mt4-hst.hpp"
#include "mt4-common.hpp"
#include "xquotes_csv.hpp"

using json = nlohmann::json;

#define PROGRAM_VERSION "1.0"
#define PROGRAM_DATE "15.04.2020"

int main(int argc, char* argv[]) {
    std::cout << "forexprostools downloader" << std::endl;
    std::cout
        << "version: " << PROGRAM_VERSION
        << " date: " << PROGRAM_DATE
        << std::endl << std::endl;

    std::string path_csv = "storages\\";
    // C:\Users\user\AppData\Roaming\MetaQuotes\Terminal\2E8DC23981084565FA3E19C061F586B2\history\RoboForex-Demo
    std::string path_hst = "C:\\Users\\user\\AppData\\Roaming\\MetaQuotes\\Terminal\\2E8DC23981084565FA3E19C061F586B2\\history\\RoboForex-Demo\\";
    //std::string path_hst = "";
    bf::create_directory(path_csv);

    std::vector<mt4_common::SymbolConfig> symbols_config;
    std::vector<std::shared_ptr<mt4_tools::MqlHst>> mql_history;
    StooqApi stooq;

    mt4_common::SymbolConfig symbol_config;
    symbol_config.symbol = "GBPUSD";
    symbol_config.digits = 5;
    symbol_config.period = 1440;
    symbols_config.push_back(symbol_config);

    /* инициализируем историю */
    std::cout << "step-1" << std::endl;
    mql_history.resize(symbols_config.size());
    for(size_t si = 0; si < symbols_config.size(); ++si) {
        mql_history[si] = std::make_shared<mt4_tools::MqlHst>(
            symbols_config[si].symbol + "SQ",
            path_hst,
            symbols_config[si].period,
            symbols_config[si].digits);
    }
    std::cout << "step-2" << std::endl;
    while(true) {
        xtime::timestamp_t timestamp = xtime::get_timestamp();
        xtime::timestamp_t restart_timestamp = timestamp + xtime::SECONDS_IN_MINUTE;
        /* загружаем данные */
        for(size_t si = 0; si < symbols_config.size(); ++si) {
            /* читаем данные из csv файла */
            std::cout << "step-3" << std::endl;
            std::vector<xquotes_common::Candle> candles_csv;
            std::string file_csv(path_csv + symbols_config[si].symbol + std::to_string(symbols_config[si].period) + ".csv");
            if(bf::check_file(file_csv)) {
                std::cout << "step-4" << std::endl;
                int err_csv = xquotes_csv::read_file(
                        file_csv,
                        false,
                        xquotes_common::DO_NOT_CHANGE_TIME_ZONE,
                        [&](xquotes_csv::Candle candle, bool is_end) {
                    if(!is_end) {
                        candles_csv.push_back(candle);
                        //std::cout << "t: " << xtime::get_str_date(candle.timestamp) << " c: " << candle.close <<  std::endl;
                    }
                });
                std::cout << "step-5" << std::endl;
                if(err_csv != xquotes_common::OK) {
                    std::cout << std::endl << "error! csv file, code: " << err_csv << std::endl;
                    return EXIT_FAILURE;
                }
            }
            std::cout << "step-6" << std::endl;

            xtime::timestamp_t timestamp_beg = xtime::get_first_timestamp_day(xtime::get_timestamp(1,1,1970));
            xtime::timestamp_t timestamp_end = xtime::get_first_timestamp_day();
            if(candles_csv.size() != 0) timestamp_beg = xtime::get_first_timestamp_day(candles_csv.back().timestamp);
            std::cout << "date: " << xtime::get_str_date(timestamp_beg) << " - " << xtime::get_str_date(timestamp_end) <<  std::endl;

            /* качаем историю */
            std::vector<xquotes_common::Candle> candles;
            StooqApi::PeriodTypes stooq_period = StooqApi::PeriodTypes::DAY;
            switch(symbols_config[si].period) {
            case xtime::MINUTES_IN_DAY:
                stooq_period = StooqApi::PeriodTypes::DAY;
                break;
            case 10080:
                stooq_period = StooqApi::PeriodTypes::WEEK;
                break;
            case 40320:
                stooq_period = StooqApi::PeriodTypes::MONTH;
                break;
            };
            std::cout << "step-7" << std::endl;
            stooq.get_historical_data(candles, symbols_config[si].symbol, stooq_period, timestamp_beg,  timestamp_end);
            if(candles_csv.size() != 0) {
                std::cout << "step-8" << std::endl;
                for(size_t i = 0; i < candles.size(); ++i) {
                    std::cout << "step-9" << std::endl;
                    if(i == 0 && xtime::get_first_timestamp_day(candles_csv.back().timestamp) == xtime::get_first_timestamp_day(candles.back().timestamp)) {
                        candles_csv.back() = candles[0];
                    } else {
                        candles_csv.push_back(candles[i]);
                    }
                }
            } else {
                std::cout << "step-10" << std::endl;
                candles_csv = candles;
            }

            /* записываем csv */
            std::cout << "step-11" << std::endl;
            std::string header_csv;
            mt4_tools::CsvTypes type_csv = mt4_tools::CsvTypes::MT4;
            int err_csv = mt4_tools::write_file(
                    file_csv,
                    header_csv,
                    candles_csv,
                    type_csv);
            std::cout << "step-12" << std::endl;
            if(err_csv != xquotes_common::OK) {
                std::cout << std::endl << "error! error! csv file, code: " << err_csv << std::endl;
                return EXIT_FAILURE;
            }

            /* обновляем hst файл */
            std::cout << "step-13" << std::endl;
            const xtime::timestamp_t last_timestamp = mql_history[si]->get_last_timestamp();
            if(last_timestamp == 0) {
                std::cout << "step-14" << std::endl;
                for(size_t i = 0; i < candles_csv.size(); ++i) {
                    mql_history[si]->add_new_candle(candles_csv[i]);
                }
            } else {
                std::cout << "step-15" << std::endl;
                for(size_t i = 0; i < candles_csv.size(); ++i) {
                    if(candles_csv[i].timestamp == last_timestamp) {
                        mql_history[si]->update_candle(candles_csv[i]);
                    } else
                    if(candles_csv[i].timestamp > last_timestamp) {
                        mql_history[si]->add_new_candle(candles_csv[i]);
                    }
                }
            }
            std::cout << "step-16" << std::endl;
        }
        std::cout << "step-17" << std::endl;
        while(xtime::get_timestamp() < restart_timestamp) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    return EXIT_FAILURE;
}

