#ifndef MT4_SETTINGS_HPP_INCLUDED
#define MT4_SETTINGS_HPP_INCLUDED

#include "mt4-common.hpp"
#include <nlohmann/json.hpp>

namespace mt4_tools {
    using json = nlohmann::json;

    /** \brief Класс настроек
     */
    class Settings {
    public:
        std::vector<mt4_common::SymbolConfig> symbols_config;
        std::string path_csv;
        std::string path_hst;// = "C:\\Users\\user\\AppData\\Roaming\\MetaQuotes\\Terminal\\2E8DC23981084565FA3E19C061F586B2\\history\\RoboForex-Demo\\";
        std::string symbol_hst_suffix;
        std::string symbol_csv_suffix;
        std::string sert_file = "curl-ca-bundle.crt";       /**< Файл сертификата */
        std::string json_settings_file;
        uint32_t update_period = 5;

        bool is_error = false;

        Settings() {};

        Settings(const int argc, char **argv) {
            /* обрабатываем аргументы командой строки */
            json j;
            bool is_default = false;
            if(!mt4_common::process_arguments(
                    argc,
                    argv,
                    [&](
                        const std::string &key,
                        const std::string &value) {
                /* аргумент json_file указываает на файл с настройками json */
                if(key == "json_settings_file" || key == "jsf" || key == "jf") {
                    json_settings_file = value;
                }
            })) {
                /* параметры не были указаны */
                if(!mt4_common::open_json_file("config.json", j)) {
                    is_error = true;
                    return;
                }
                is_default = true;
            }

            if(!is_default && !mt4_common::open_json_file(json_settings_file, j)) {
                is_error = true;
                return;
            }

            /* разбираем json сообщение */
            try {
                if(j["sert_file"] != nullptr) sert_file = j["sert_file"];
                if(j["update_period"] != nullptr) update_period = j["update_period"];
                if(j["symbol_hst_suffix"] != nullptr) symbol_hst_suffix = j["symbol_hst_suffix"];
                if(j["symbol_csv_suffix"] != nullptr) symbol_csv_suffix = j["symbol_csv_suffix"];
                if(j["path_csv"] != nullptr) path_csv = j["path_csv"];
                if(j["path_hst"] != nullptr) path_hst = j["path_hst"];
                if(j["symbols"] != nullptr && j["symbols"].is_array()) {
                    const size_t symbols_size = j["symbols"].size();
                    for(size_t i = 0; i < symbols_size; ++i) {
                        mt4_common::SymbolConfig symbol_config;
                        symbol_config.symbol = j["symbols"][i]["symbol"];
                        symbol_config.period = j["symbols"][i]["period"];
                        symbol_config.digits = j["symbols"][i]["digits"];
                        symbols_config.push_back(symbol_config);
                    }
                }
            }
            catch(const json::parse_error& e) {
                std::cerr << "mt4_tools::Settings parser error (json::parse_error), what: " << std::string(e.what()) << std::endl;
                is_error = true;
            }
            catch(const json::out_of_range& e) {
                std::cerr << "mt4_tools::Settings parser error (json::out_of_range), what: " << std::string(e.what()) << std::endl;
                is_error = true;
            }
            catch(const json::type_error& e) {
                std::cerr << "mt4_tools::Settings parser error (json::type_error), what: " << std::string(e.what()) << std::endl;
                is_error = true;
            }
            catch(...) {
                std::cerr << "mt4_tools::Settings parser error" << std::endl;
                is_error = true;
            }
            if(symbols_config.size() == 0) is_error = true;
        }
    };
}

#endif // MT4_SETTINGS_HPP_INCLUDED
