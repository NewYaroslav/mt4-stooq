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
#ifndef MT4_STOOQ_HPP_INCLUDED
#define MT4_STOOQ_HPP_INCLUDED

#include <iostream>
#include <curl/curl.h>
#include <xtime.hpp>
#include <thread>
#include <string>
#include <vector>
#include "xquotes_common.hpp"
#include "nlohmann/json.hpp"
#include "gzip/decompress.hpp"

/** \brief Класс для работы с https://stooq.com
 */
class StooqApi {
public:

    /// Варианты состояния ошибок
    enum ErrorType {
        OK = 0,                             ///< Ошибки нет
        CURL_CANNOT_BE_INIT = -1,           ///< CURL не может быть инициализирован
        CONTENT_ENCODING_NOT_SUPPORT = -2,  ///< Тип кодирования контента не поддерживается
        PARSER_ERROR = -3,                  ///< Ошибка парсера ответа от сервера
        JSON_PARSER_ERROR = -4,             ///< Ошибка парсера JSON
        NO_ANSWER = -5,                     ///< Нет ответа
        DATA_NOT_AVAILABLE = -6,            ///< Данные не доступны
        CURL_REQUEST_FAILED = -7,           ///< Ошибка запроса на сервер. Любой код статуса, который не равен 200, будет возвращать эту ошибку
        LIMITING_NUMBER_REQUESTS = -8,      ///< Нарушение ограничения скорости запроса.
        IP_BLOCKED = -9,                    ///< IP-адрес был автоматически заблокирован для продолжения отправки запросов после получения 429 кодов.
        WAF_LIMIT = -10,                    ///< нарушении лимита WAF (брандмауэр веб-приложений).
        NO_RESPONSE_WAITING_PERIOD = -11,
        INVALID_PARAMETER = -12,
        NO_PRICE_STREAM_SUBSCRIPTION = -13,
        INVALID_TIMESTAMP = -1021,                  /**< Временная метка для этого запроса находится за пределами recvWindow или Временная метка для этого запроса была на 1000 мс раньше времени сервера. */
        NO_SUCH_ORDER = -2013,                      /**< Заказ не существует */
        ORDER_WOULD_IMMEDIATELY_TRIGGER = -2021,    /**< Заказ сразу сработает. */
        NO_NEED_TO_CHANGE_MARGIN_TYPE = -4046,      /**< Нет необходимости изменения типа маржи */
        NO_NEED_TO_CHANGE_POSITION_SIDE = -4059,    /**< Нет необходимости менять положение стороны. */
        POSITION_SIDE_NOT_MATCH = -4061,            /**< Сторона позиции ордера не соответствует настройке пользователя. */
        POSITION_SIDE_CHANGE_EXISTS_QUANTITY = -4068,/**< Сторона позиции не может быть изменена, если существует позиция */
    };

private:
    std::string point = "https://stooq.com";
    std::string sert_file = "curl-ca-bundle.crt";       /**< Файл сертификата */
    std::string cookie_file = "binance-sapi.cookie";    /**< Файл cookie */

    char error_buffer[CURL_ERROR_SIZE];
    static const int TIME_OUT = 60;     /**< Время ожидания ответа сервера для разных запросов */

    /** \brief Класс для хранения Http заголовков
     */
    class HttpHeaders {
    private:
        struct curl_slist *http_headers = nullptr;
    public:

        HttpHeaders() {};

        HttpHeaders(std::vector<std::string> headers) {
            for(size_t i = 0; i < headers.size(); ++i) {
                add_header(headers[i]);
            }
        };

        void add_header(const std::string &header) {
            http_headers = curl_slist_append(http_headers, header.c_str());
        }

        void add_header(const std::string &key, const std::string &val) {
            std::string header(key + ": " + val);
            http_headers = curl_slist_append(http_headers, header.c_str());
        }

        ~HttpHeaders() {
            if(http_headers != nullptr) {
                curl_slist_free_all(http_headers);
                http_headers = nullptr;
            }
        };

        inline struct curl_slist *get() {
            return http_headers;
        }
    };

    /** \brief Callback-функция для обработки ответа
     * Данная функция нужна для внутреннего использования
     */
    static int stooq_writer(char *data, size_t size, size_t nmemb, void *userdata) {
        int result = 0;
        std::string *buffer = (std::string*)userdata;
        if(buffer != NULL) {
            buffer->append(data, size * nmemb);
            result = size * nmemb;
        }
        return result;
    }

    /** \brief Парсер строки, состоящей из пары параметров
     *
     * \param value Строка
     * \param one Первое значение
     * \param two Второе значение
     */
    static void parse_pair(std::string value, std::string &one, std::string &two) {
        if(value.back() != ' ' || value.back() != '\n' || value.back() != ' ') value += " ";
        std::size_t start_pos = 0;
        while(true) {
            std::size_t found_beg = value.find_first_of(" \t\n", start_pos);
            if(found_beg != std::string::npos) {
                std::size_t len = found_beg - start_pos;
                if(len > 0) {
                    if(start_pos == 0) one = value.substr(start_pos, len);
                    else two = value.substr(start_pos, len);
                }
                start_pos = found_beg + 1;
            } else break;
        }
    }

    /** \brief Callback-функция для обработки HTTP Header ответа
     * Данный метод нужен, чтобы определить, какой тип сжатия данных используется (или сжатие не используется)
     * Данный метод нужен для внутреннего использования
     */
    static int stooq_header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
        size_t buffer_size = nitems * size;
        std::map<std::string,std::string> *headers = (std::map<std::string,std::string>*)userdata;
        std::string str_buffer(buffer, buffer_size);
        std::string key, val;
        parse_pair(str_buffer, key, val);
        headers->insert({key, val});
        return buffer_size;
    }

    enum class TypesRequest {
        REQ_GET = 0,
        REQ_POST = 1,
        REQ_PUT = 2,
        REQ_DELETE = 3
    };

    /** \brief Инициализация CURL
     *
     * Данная метод является общей инициализацией для разного рода запросов
     * Данный метод нужен для внутреннего использования
     * \param url URL запроса
     * \param body Тело запроса
     * \param response Ответ сервера
     * \param http_headers Заголовки HTTP
     * \param timeout Таймаут
     * \param writer_callback Callback-функция для записи данных от сервера
     * \param header_callback Callback-функция для обработки заголовков ответа
     * \param is_use_cookie Использовать cookie файлы
     * \param is_clear_cookie Очистить cookie файлы
     * \param type_req Использовать POST, GET и прочие запросы
     * \return вернет указатель на CURL или NULL, если инициализация не удалась
     */
    CURL *init_curl(
            const std::string &url,
            const std::string &body,
            std::string &response,
            struct curl_slist *http_headers,
            const int timeout,
            int (*writer_callback)(char*, size_t, size_t, void*),
            int (*header_callback)(char*, size_t, size_t, void*),
            void *userdata,
            const bool is_use_cookie = true,
            const bool is_clear_cookie = false,
            const TypesRequest type_req = TypesRequest::REQ_POST) {
        CURL *curl = curl_easy_init();
        if(!curl) return NULL;
        curl_easy_setopt(curl, CURLOPT_CAINFO, sert_file.c_str());
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        //curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        if(type_req == TypesRequest::REQ_POST) curl_easy_setopt(curl, CURLOPT_POST, 1L);
        else if(type_req == TypesRequest::REQ_GET) curl_easy_setopt(curl, CURLOPT_POST, 0);
        else if(type_req == TypesRequest::REQ_PUT) curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        else if(type_req == TypesRequest::REQ_DELETE) curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writer_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout); // выход через N сек
        if(is_use_cookie) {
            if(is_clear_cookie) curl_easy_setopt(curl, CURLOPT_COOKIELIST, "ALL");
            else curl_easy_setopt(curl, CURLOPT_COOKIEFILE, cookie_file.c_str()); // запускаем cookie engine
            curl_easy_setopt(curl, CURLOPT_COOKIEJAR, cookie_file.c_str()); // запишем cookie после вызова curl_easy_cleanup
        }
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, userdata);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http_headers);
        if (type_req == TypesRequest::REQ_POST ||
            type_req == TypesRequest::REQ_PUT ||
            type_req == TypesRequest::REQ_DELETE)
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        //curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
        return curl;
    }

    /** \brief Обработать ответ сервера
     * \param curl Указатель на структуру CURL
     * \param headers Заголовки, которые были приняты
     * \param buffer Буфер с ответом сервера
     * \param response Итоговый ответ, который будет возвращен
     * \return Код ошибки
     */
    int process_server_response(CURL *curl, std::map<std::string,std::string> &headers, std::string &buffer, std::string &response) {
        CURLcode result = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        if(result == CURLE_OK) {
            if(headers.find("Content-Encoding:") != headers.end()) {
                std::string content_encoding = headers["Content-Encoding:"];
                if(content_encoding.find("gzip") != std::string::npos) {
                    if(buffer.size() == 0) return NO_ANSWER;
                    const char *compressed_pointer = buffer.data();
                    response = gzip::decompress(compressed_pointer, buffer.size());
                } else
                if(content_encoding.find("identity") != std::string::npos) {
                    response = buffer;
                } else {
                    if(response_code != 200) return CURL_REQUEST_FAILED;
                    return CONTENT_ENCODING_NOT_SUPPORT;
                }
            } else
            if(headers.find("content-encoding:") != headers.end()) {
                std::string content_encoding = headers["content-encoding:"];
                if(content_encoding.find("gzip") != std::string::npos) {
                    if(buffer.size() == 0) return NO_ANSWER;
                    const char *compressed_pointer = buffer.data();
                    response = gzip::decompress(compressed_pointer, buffer.size());
                } else
                if(content_encoding.find("identity") != std::string::npos) {
                    response = buffer;
                } else {
                    if(response_code != 200) return CURL_REQUEST_FAILED;
                    return CONTENT_ENCODING_NOT_SUPPORT;
                }
            } else {
                response = buffer;
                if(response_code != 200) return CURL_REQUEST_FAILED;
            }
            if(response_code != 200) return CURL_REQUEST_FAILED;
        }
        return result;
    }

    /** \brief GET запрос
     *
     * Данный метод нужен для внутреннего использования
     * \param url URL сообщения
     * \param body Тело сообщения
     * \param http_headers Заголовки
     * \param response Ответ
     * \param is_clear_cookie Очистить cookie
     * \param timeout Время ожидания ответа
     * \return код ошибки
     */
    int get_request(
            const std::string &url,
            const std::string &body,
            struct curl_slist *http_headers,
            std::string &response,
            const bool is_use_cookie = true,
            const bool is_clear_cookie = false,
            const int timeout = TIME_OUT) {
        //int content_encoding = 0;   // Тип кодирования сообщения
        std::map<std::string,std::string> headers;
        std::string buffer;
        CURL *curl = init_curl(
            url,
            body,
            buffer,
            http_headers,
            timeout,
            stooq_writer,
            stooq_header_callback,
            &headers,
            is_use_cookie,
            is_clear_cookie,
            TypesRequest::REQ_GET);

        if(curl == NULL) return CURL_CANNOT_BE_INIT;
        int err = process_server_response(curl, headers, buffer, response);
        curl_easy_cleanup(curl);
        return err;
    }

    std::string to_lower_case(const std::string &s){
        std::string temp = s;
        std::transform(temp.begin(), temp.end(), temp.begin(), [](char ch) {
            return std::use_facet<std::ctype<char>>(std::locale()).tolower(ch);
        });
        return temp;
    }

    std::string to_upper_case(const std::string &s){
        std::string temp = s;
        std::transform(temp.begin(), temp.end(), temp.begin(), [](char ch) {
            return std::use_facet<std::ctype<char>>(std::locale()).toupper(ch);
        });
        return temp;
    }

    int get_request_none_security(std::string &response, const std::string &url, const uint64_t weight = 1) {
        const std::string body;
        //check_request_limit(weight);
        HttpHeaders http_headers({
            //"Accept-Encoding: gzip",
            "Content-Type: application/json"});
        int err = get_request(url, body, http_headers.get(), response, false, false);
        if(err != OK) {

        }
        return err;
    }

    void parse_line(std::string line, std::vector<std::string> &output_list) {
        if(line.back() != '\n') line += "\n";
        std::size_t start_pos = 0;
        while(true) {
            std::size_t found_beg = line.find_first_of("\n", start_pos);
            if(found_beg != std::string::npos) {
                std::size_t len = found_beg - start_pos;
                if(len > 0) {
                    output_list.push_back(line.substr(start_pos, len));
                }
                start_pos = found_beg + 1;
            } else break;
        }
    }

    void parse_word(std::string line, std::vector<std::string> &output_list) {
        if(line.back() != ',' && line.back() != '-') line += ",";
        std::size_t start_pos = 0;
        while(true) {
            std::size_t found_beg = line.find_first_of(",-\n", start_pos);
            if(found_beg != std::string::npos) {
                std::size_t len = found_beg - start_pos;
                if(len > 0) {
                    //if(output_list.size() == 0 && line.size() > 3 && line.substr(0, 2) == "~/") {
                    //    output_list.push_back(line.substr(0, 2));
                    //} else
                    //if(output_list.size() == 0 && line.size() > 2 && line.substr(0, 1) == "/") {
                    //    output_list.push_back(line.substr(0, 1));
                    //}
                    output_list.push_back(line.substr(start_pos, len));
                }
                start_pos = found_beg + 1;
            } else break;
        }
    }

    void parse_history(
            std::vector<xquotes_common::Candle> &candles,
            std::string &response) {
        try {
            std::vector<std::string> item_list;
            parse_line(response, item_list);
            const size_t size_data = item_list.size();
            for(size_t l = 1; l < size_data; ++l) {
                std::vector<std::string> item_word;
                parse_word(item_list[l], item_word);
                xquotes_common::Candle candle;
                if(item_word.size() >= 7) {
                    const int year = std::atoi(item_word[0].c_str());
                    const int month = std::atoi(item_word[1].c_str());
                    const int day = std::atoi(item_word[2].c_str());
                    candle.timestamp = xtime::get_timestamp(day, month, year);
                    // Open High Low Close
                    candle.open = std::atof(item_word[3].c_str());
                    candle.high = std::atof(item_word[4].c_str());
                    candle.low = std::atof(item_word[5].c_str());
                    candle.close = std::atof(item_word[6].c_str());
                    if(item_word.size() >= 8) {
                        candle.volume = std::atof(item_word[7].c_str());
                    }
                    candles.push_back(candle);
                }
            }
        } catch(...) {}
    }

public:

    StooqApi(const std::string &user_sert_file = "curl-ca-bundle.crt") {
        sert_file = user_sert_file;
        curl_global_init(CURL_GLOBAL_ALL);
    };

    ~StooqApi() {};

    enum class PeriodTypes {
        DAY,
        WEEK,
        MONTH,
        QUARTER,
        YEAR
    };

    /** \brief Получить исторические данные
     *
     * \param candles Массив баров
     * \param symbol Имя символа
     * \param period Период
     * \param limit Ограничение количества баров
     * \return Код ошибки
     */
    int get_historical_data(
            std::vector<xquotes_common::Candle> &candles,
            const std::string &symbol,
            const PeriodTypes period,
            const xtime::timestamp_t start_date,
            const xtime::timestamp_t stop_date) {
        std::string url(point);
        std::string response;
        url += "/q/d/l/?";
        url += "s=";
        url += to_lower_case(symbol);
        // 20200818
        url += "&d1=";
        url += xtime::to_string("%YYYY%MM%DD",start_date);
        url += "&d2=";
        url += xtime::to_string("%YYYY%MM%DD",stop_date);
        url += "&i=";
        if(period == PeriodTypes::DAY) url += "d";
        else if(period == PeriodTypes::WEEK) url += "w";
        else if(period == PeriodTypes::MONTH) url += "m";
        else if(period == PeriodTypes::YEAR) url += "y";
        //std::cout << url << std::endl;
        int err = get_request_none_security(response, url);
        //std::cout << response << std::endl;
        if(err != OK) return err;
        parse_history(candles, response);
        return OK;
    }
};

#endif // FOREXPROSTOOLSAPI_HPP_INCLUDED
