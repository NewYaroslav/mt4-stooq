#ifndef MT4_CSV_HPP_INCLUDED
#define MT4_CSV_HPP_INCLUDED

#include "xquotes_common.hpp"
#include "banana_filesystem.hpp"
#include "xtime.hpp"
#include <functional>
#include <iostream>

namespace mt4_tools {

    enum class CsvTypes {
        MT4,
        MT5,
        DUKASCOPY
    };

    /** \brief Записать файл
     * \param file_name Имя csv файла, куда запишем данные
     * \param header Заголовок csv файла
     * \param is_write_header Флаг записи заголовка csv файла. Если true, заголовок будет записан
     * \param start_timestamp Метка времени начала записи. Метка времени должна находится в том же часовом поясе, в котором находится источник данных цен!
     * \param stop_timestamp Метка времени завершения записи. Данная метка времени будет включена в массив цен!
     * \param type_csv Тип csv файла (MT4, MT5, DUKASCOPY)
     * \param time_zone Изменить часовой пояс меток времени
     * (DO_NOT_CHANGE_TIME_ZONE, CET_TO_GMT, EET_TO_GMT, GMT_TO_CET, GMT_TO_EET)
     * \param decimal_places количество знаков после запятой
     * \param f лямбда-функция для получения бара или свечи по метке времени, должна вернуть false в случае завершения
     * Лямбда функция может пропускать запись по своему усмотрению. Для этого достаточно вернуть false.
     * \return вернет 0 в случае успеха, иначе см. код ошибок в xquotes_common.hpp
     */
    int write_file(
            const std::string &file_name,
            const std::string &header,
            const std::vector<xquotes_common::Candle> &candles,
            const CsvTypes type_csv) {
        //std::ofstream file(file_name);
        if(!bf::check_file(file_name)) {
            std::fstream file(file_name, std::ios::out | std::ios::app);
            file.close();
        }
        std::fstream file(file_name, std::ios::in | std::ios::out | std::ios::ate);
        if(!file.is_open()) {
            return xquotes_common::FILE_CANNOT_OPENED;
        }
        file.clear();
        file.seekg(0, std::ios::beg);
        file.clear();
        const int decimal_places = xquotes_common::get_decimal_places(candles);
        std::string str_price = "%." + std::to_string(decimal_places) + "f";
        std::string sprintf_param =
            // пример MT4: 1971.01.04,00:00,0.53690,0.53690,0.53690,0.53690,1
            type_csv == CsvTypes::MT4 ? "%.4d.%.2d.%.2d,%.2d:%.2d," + str_price + "," + str_price + "," + str_price + "," + str_price + ",%d" :
            /* пример MT5: 2007.02.12	11:36:00	0.90510	0.90510	0.90500	0.90500	4	0	100
             * спред и реальный объем придется заполнить 0
             */
            type_csv == CsvTypes::MT5 ? "%.4d.%.2d.%.2d\t%.2d:%.2d:%.2d\t" + str_price + "\t" + str_price + "\t" + str_price + "\t" + str_price + "\t" + "%d\t0\t0" :
            // пример DUKASCOPY: 01.01.2017 00:00:00.000,1150.312,1150.312,1150.312,1150.312,0
            type_csv == CsvTypes::DUKASCOPY ? "%.2d.%.2d.%.4d %.2d:%.2d:%.2d.000," + str_price + "," + str_price + "," + str_price + "," + str_price + "," + "%f" :
            "%.4d.%.2d.%.2d,%.2d:%.2d," + str_price + "," + str_price + "," + str_price + "," + str_price + ",%d";

        if(header.size() != 0) file << header << std::endl;

        const int BUFFER_SIZE = 1024;
        char buffer[BUFFER_SIZE];

        for(size_t i = 0; i < candles.size(); ++i) {
            xquotes_common::Candle candle = candles[i];
            xtime::DateTime date_time(candle.timestamp);
            std::fill(buffer, buffer + BUFFER_SIZE, '\0');
            switch(type_csv) {
            case CsvTypes::MT4:
            default:
                sprintf(
                    buffer,
                    sprintf_param.c_str(),
                    date_time.year,
                    date_time.month,
                    date_time.day,
                    date_time.hour,
                    date_time.minute,
                    candle.open,
                    candle.high,
                    candle.low,
                    candle.close,
                    (int)candle.volume);
                break;
            case CsvTypes::MT5:
                sprintf(
                    buffer,
                    sprintf_param.c_str(),
                    date_time.year,
                    date_time.month,
                    date_time.day,
                    date_time.hour,
                    date_time.minute,
                    date_time.second,
                    candle.open,
                    candle.high,
                    candle.low,
                    candle.close,
                    (int)candle.volume);
                break;
            case CsvTypes::DUKASCOPY:
                sprintf(
                    buffer,
                    sprintf_param.c_str(),
                    date_time.day,
                    date_time.month,
                    date_time.year,
                    date_time.hour,
                    date_time.minute,
                    date_time.second,
                    candle.open,
                    candle.high,
                    candle.low,
                    candle.close,
                    candle.volume);
                break;
            }
            std::string line(buffer);
            file << line << std::endl;
        }
        file.close();
        return xquotes_common::OK;
    }
};
#endif // MT4-CSV_HPP_INCLUDED
