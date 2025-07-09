/*
 * Препроцессор файлов C++
 * Программа разворачивает директивы #include в исходных файлах,
 * заменяя их содержимым включаемых файлов
 */

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
using filesystem::path;

// Пользовательский литерал для создания объектов path из строковых литералов
path operator""_p(const char* data, size_t sz) {
    return path(data, data + sz);
}

/**
 * Рекурсивно обрабатывает файл, разворачивая директивы #include
 * 
 * @param current_file - текущий обрабатываемый файл
 * @param output - выходной поток для записи результата
 * @param include_dirs - список директорий для поиска заголовочных файлов
 * @param source_file - исходный файл (для отображения ошибок)
 * @param source_line - номер строки в исходном файле (для отображения ошибок)
 * @return true в случае успеха, false при ошибке
 */
bool ProcessInclude(const path &current_file, ofstream &output, const vector<path> &include_dirs, const path &source_file = "", int source_line = 0) {
    // Попытка открыть текущий файл для чтения
    ifstream input(current_file);
    if (!input.is_open()) {
        // Вывод ошибки, если файл не найден
        if (!source_file.empty()) {
            cout << "unknown include file " << current_file.filename().string() 
                 << " at file " << source_file.string() 
                 << " at line " << source_line << endl;
        }
        return false;
    }

    // Регулярные выражения для поиска директив include
    // Локальные заголовки: #include "file.h"
    static const regex include_local(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    // Системные заголовки: #include <file.h>
    static const regex include_global(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    
    string line;
    int line_number = 0;
    bool success = true;

    // Обработка файла построчно
    while (getline(input, line)) {
        line_number++;
        smatch match;

        // Обработка локальных заголовков (#include "...")
        if (regex_search(line, match, include_local)) {
            path include_path = match[1].str(); // Извлекаем имя файла
            path current_dir = current_file.parent_path(); // Директория текущего файла
            path full_path = current_dir / include_path; // Полный путь к включаемому файлу

            // Поиск файла сначала относительно текущей директории
            if (!filesystem::exists(full_path)) {
                bool found = false;

                // Поиск в директориях include
                for (const auto &dir : include_dirs) {
                    full_path = dir / include_path;
                    if (filesystem::exists(full_path)) {
                        found = true;
                        break;
                    }
                }

                // Ошибка, если файл не найден
                if (!found) {
                    cout << "unknown include file " << include_path.string() 
                         << " at file " << current_file.string() 
                         << " at line " << line_number << endl;
                    success = false;
                    break;
                }
            }

            // Рекурсивная обработка найденного файла
            if (!ProcessInclude(full_path, output, include_dirs, current_file, line_number)) {
                success = false;
                break;
            }
        } 
        // Обработка системных заголовков (#include <...>)
        else if (regex_search(line, match, include_global)) {
            path include_path = match[1].str(); // Извлекаем имя файла
            bool found = false;
            path full_path;

            // Поиск только в директориях include (не относительно текущей)
            for (const auto &dir : include_dirs) {
                full_path = dir / include_path;
                if (filesystem::exists(full_path)) {
                    found = true;
                    break;
                }
            }

            // Ошибка, если файл не найден
            if (!found) {
                cout << "unknown include file " << include_path.string() 
                     << " at file " << current_file.string() 
                     << " at line " << line_number << endl;
                success = false;
                break;
            }

            // Рекурсивная обработка найденного файла
            if (!ProcessInclude(full_path, output, include_dirs, current_file, line_number)) {
                success = false;
                break;
            }
        } 
        // Если строка не содержит директиву include, копируем её как есть
        else {
            output << line << endl;
        }
    }

    return success;
}

/**
 * Главная функция препроцессинга
 * Обрабатывает входной файл и создаёт выходной файл с развёрнутыми include
 * 
 * @param input_file - путь к входному файлу
 * @param output_file - путь к выходному файлу
 * @param include_dirs - список директорий для поиска заголовочных файлов
 * @return true в случае успеха, false при ошибке
 */
bool Preprocess(const path& input_file, const path& output_file,
                const vector<path>& include_dirs) {
    // Проверка возможности открытия входного файла
    ifstream input(input_file);
    if (!input.is_open()) {
        cout << "Ошибка: Не удалось открыть входной файл: " << input_file.string() << endl;
        return false;
    }

    // Проверка возможности создания выходного файла
    ofstream output(output_file);
    if (!output.is_open()) {
        cout << "Ошибка: Не удалось открыть выходной файл: " << output_file.string() << endl;
        return false;
    }

    // Запуск обработки файла
    return ProcessInclude(input_file, output, include_dirs);
}

/**
 * Вспомогательная функция для чтения всего содержимого файла в строку
 * 
 * @param file - путь к файлу
 * @return содержимое файла в виде строки
 */
string GetFileContents(const string& file) {
    ifstream stream(file);
    return {istreambuf_iterator<char>(stream), istreambuf_iterator<char>()};
}

/**
 * Функция тестирования препроцессора
 * Создаёт тестовую файловую структуру и проверяет корректность работы
 */
void Test() {
    error_code err;
    
    // Очистка и создание тестовой структуры директорий
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    // Создание тестового основного файла sources/a.cpp
    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n" // Этот include должен вызвать ошибку
                "}\n"s;
    }
    
    // Создание тестового заголовка sources/dir1/b.h
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    
    // Создание тестового заголовка sources/dir1/subdir/c.h
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    
    // Создание тестового заголовка sources/dir1/d.h
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    
    // Создание системного заголовка sources/include1/std1.h
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    
    // Создание системного заголовка sources/include2/lib/std2.h
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    // Запуск препроцессинга (ожидается неудача из-за dummy.txt)
    assert(!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                       {"sources"_p / "include1"_p, "sources"_p / "include2"_p}));

    // Ожидаемый результат после успешного препроцессинга
    ostringstream test_out;
    test_out << "// this comment before include\n"
                "// text from b.h before include\n"
                "// text from c.h before include\n"
                "// std1\n"
                "// text from c.h after include\n"
                "// text from b.h after include\n"
                "// text between b.h and c.h\n"
                "// text from d.h before include\n"
                "// std2\n"
                "// text from d.h after include\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"s;

    // Проверка корректности результата
    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

/**
 * Главная функция программы
 * Запускает тестирование препроцессора
 */
int main() {
    Test();
}
