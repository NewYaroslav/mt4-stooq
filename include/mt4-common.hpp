#ifndef MT4_COMMON_HPP_INCLUDED
#define MT4_COMMON_HPP_INCLUDED

namespace mt4_common {
    using json = nlohmann::json;

    /** \brief Параметры символа
     */
    class SymbolConfig {
    public:
        std::string symbol;
        uint32_t digits = 5;
        uint32_t period = 1440;

        SymbolConfig() {};
    };

    /** \brief Обработать аргументы
     *
     * Данная функция обрабатывает аргументы от командной строки, возвращая
     * результат как пара ключ - значение.
     * \param argc количество аргуметов
     * \param argv вектор аргументов
     * \param f лябмда-функция для обработки аргументов командной строки
     * \return Вернет true если ошибок нет
     */
    bool process_arguments(
        const int argc,
        char **argv,
        std::function<void(
            const std::string &key,
            const std::string &value)> f) noexcept {
        if(argc <= 1) return false;
        bool is_error = true;
        for(int i = 1; i < argc; ++i) {
            std::string key = std::string(argv[i]);
            if(key.size() > 0 && (key[0] == '-' || key[0] == '/')) {
                uint32_t delim_offset = 0;
                if(key.size() > 2 && (key.substr(2) == "--") == 0) delim_offset = 1;
                std::string value;
                if((i + 1) < argc) value = std::string(argv[i + 1]);
                is_error = false;
                if(f != nullptr) f(key.substr(delim_offset), value);
            }
        }
        return !is_error;
    }

    /** \brief Открыть файл JSON
     *
     * Данная функция прочитает файл с JSON и запишет данные в JSON структуру
     * \param file_name Имя файла
     * \param auth_json Структура JSON с данными из файла
     * \return Вернет true в случае успешного завершения
     */
    bool open_json_file(const std::string &file_name, json &auth_json) {
        std::ifstream auth_file(file_name);
        if(!auth_file) {
            std::cerr << "open file " << file_name << " error" << std::endl;
            return false;
        }
        try {
            auth_file >> auth_json;
        }
        catch (json::parse_error &e) {
            std::cerr << "json parser error: " << std::string(e.what()) << std::endl;
            auth_file.close();
            return false;
        }
        catch (std::exception e) {
            std::cerr << "json parser error: " << std::string(e.what()) << std::endl;
            auth_file.close();
            return false;
        }
        catch(...) {
            std::cerr << "json parser error" << std::endl;
            auth_file.close();
            return false;
        }
        auth_file.close();
        return true;
    }
}

#endif // MT4_COMMON_HPP_INCLUDED
