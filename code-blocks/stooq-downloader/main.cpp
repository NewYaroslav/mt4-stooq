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
#include "mt4-settings.hpp"
#include "xquotes_csv.hpp"

using json = nlohmann::json;

#define PROGRAM_VERSION "1.2"
#define PROGRAM_DATE "28.08.2020"

int main(int argc, char* argv[]) {
    std::cout << "stooq downloader" << std::endl;
    std::cout
        << "version: " << PROGRAM_VERSION
        << " date: " << PROGRAM_DATE
        << std::endl << std::endl;
    //if(xtime::get_timestamp() > xtime::get_timestamp(1,9,2020)) {
     //   std::cout << "XXL LOL" << std::endl;
     //   std::system("pause");
    //    return EXIT_FAILURE;
    //}

    mt4_tools::Settings settings(argc, argv);
    if(settings.is_error) {
        std::cout << "settings error! exit" << std::endl;
        std::system("pause");
        return EXIT_FAILURE;
    }

    if(settings.path_csv.size() != 0) settings.path_csv += "\\";
    if(settings.path_hst.size() != 0) settings.path_hst += "\\";
    if(settings.path_csv.size() != 0) bf::create_directory(settings.path_csv);
    if(settings.path_hst.size() != 0) bf::create_directory(settings.path_hst);
    //std::string path_hst = "C:\\Users\\user\\AppData\\Roaming\\MetaQuotes\\Terminal\\2E8DC23981084565FA3E19C061F586B2\\history\\RoboForex-Demo\\";

    std::vector<std::shared_ptr<mt4_tools::MqlHst>> mql_history;
    StooqApi stooq;

    /* инициализируем историю */
    std::cout << "init mql history" << std::endl;
    mql_history.resize(settings.symbols_config.size());
    for(size_t si = 0; si < settings.symbols_config.size(); ++si) {
        mql_history[si] = std::make_shared<mt4_tools::MqlHst>(
            settings.symbols_config[si].symbol + settings.symbol_hst_suffix,
            settings.path_hst,
            settings.symbols_config[si].period,
            settings.symbols_config[si].digits);
    }
    while(true) {
        std::cout << "update start" << std::endl;
        xtime::timestamp_t timestamp = xtime::get_timestamp();
        xtime::timestamp_t restart_timestamp = timestamp - (timestamp % (settings.update_period * xtime::SECONDS_IN_MINUTE)) + (settings.update_period * xtime::SECONDS_IN_MINUTE);
        /* загружаем данные */
        for(size_t si = 0; si < settings.symbols_config.size(); ++si) {
            /* читаем данные из csv файла */
            std::vector<xquotes_common::Candle> candles_csv;
            std::string file_csv(settings.path_csv + settings.symbols_config[si].symbol + settings.symbol_csv_suffix + std::to_string(settings.symbols_config[si].period) + ".csv");
            if(bf::check_file(file_csv)) {
                int err_csv = xquotes_csv::read_file(
                        file_csv,
                        false,
                        xquotes_common::DO_NOT_CHANGE_TIME_ZONE,
                        [&](xquotes_csv::Candle candle, bool is_end) {
                    if(!is_end) {
                        candles_csv.push_back(candle);
                    }
                });
                if(err_csv != xquotes_common::OK) {
                    std::cout << settings.symbols_config[si].symbol << " error read csv file, code: " << err_csv << std::endl;
                    return EXIT_FAILURE;
                }
            }

            xtime::timestamp_t timestamp_beg = xtime::get_first_timestamp_day(xtime::get_timestamp(1,1,1970));
            xtime::timestamp_t timestamp_end = xtime::get_first_timestamp_day();
            if(candles_csv.size() != 0) timestamp_beg = xtime::get_first_timestamp_day(candles_csv.back().timestamp);
            std::cout << settings.symbols_config[si].symbol << " download date: " << xtime::get_str_date(timestamp_beg) << " - " << xtime::get_str_date(timestamp_end) <<  std::endl;

            /* качаем историю */
            std::vector<xquotes_common::Candle> candles;
            StooqApi::PeriodTypes stooq_period = StooqApi::PeriodTypes::DAY;
            switch(settings.symbols_config[si].period) {
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

            stooq.get_historical_data(candles, settings.symbols_config[si].symbol, stooq_period, timestamp_beg,  timestamp_end);
            if(candles_csv.size() != 0) {
                for(size_t i = 0; i < candles.size(); ++i) {
                    if(i == 0 && xtime::get_first_timestamp_day(candles_csv.back().timestamp) == xtime::get_first_timestamp_day(candles.back().timestamp)) {
                        candles_csv.back() = candles[0];
                    } else {
                        candles_csv.push_back(candles[i]);
                    }
                }
            } else {
                candles_csv = candles;
            }

            if(candles_csv.size() > 0) std::cout << settings.symbols_config[si].symbol << " write date: " << xtime::get_str_date(candles_csv.front().timestamp) << " - " << xtime::get_str_date(candles_csv.back().timestamp) <<  std::endl;
            else std::cout << settings.symbols_config[si].symbol << " write date: null" << std::endl;

            /* записываем csv */
            std::string header_csv;
            mt4_tools::CsvTypes type_csv = mt4_tools::CsvTypes::MT4;
            int err_csv = mt4_tools::write_file(
                    file_csv,
                    header_csv,
                    candles_csv,
                    type_csv);
            if(err_csv != xquotes_common::OK) {
                std::cout << settings.symbols_config[si].symbol << " error write csv file, code: " << err_csv << std::endl;
                return EXIT_FAILURE;
            }

            /* обновляем hst файл */
            const xtime::timestamp_t last_timestamp = mql_history[si]->get_last_timestamp();
            if(last_timestamp == 0) {
                for(size_t i = 0; i < candles_csv.size(); ++i) {
                    mql_history[si]->add_new_candle(candles_csv[i]);
                }
            } else {
                for(size_t i = 0; i < candles_csv.size(); ++i) {
                    if(candles_csv[i].timestamp == last_timestamp) {
                        mql_history[si]->update_candle(candles_csv[i]);
                    } else
                    if(candles_csv[i].timestamp > last_timestamp) {
                        mql_history[si]->add_new_candle(candles_csv[i]);
                    }
                }
            }
        }
        std::cout << "update completed " << xtime::get_str_date_time(xtime::get_timestamp()) << std::endl;
        std::cout << "next update " << xtime::get_str_date_time(restart_timestamp) << std::endl;
        while(xtime::get_timestamp() < restart_timestamp) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
    return EXIT_FAILURE;
}

