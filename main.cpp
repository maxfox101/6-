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

path operator""_p(const char* data, size_t sz) {
    return path(data, data + sz);
}

bool ProcessInclude(const path &current_file, ofstream &output, const vector<path> &include_dirs, const path &source_file = "", int source_line = 0) {
    ifstream input(current_file);
    if (!input.is_open()) {
        if (!source_file.empty()) {
            cout << "unknown include file " << current_file.filename().string() 
                 << " at file " << source_file.string() 
                 << " at line " << source_line << endl;
        }
        return false;
    }

    static const regex include_local(R"/(\s*#\s*include\s*"([^"]*)"\s*)/");
    static const regex include_global(R"/(\s*#\s*include\s*<([^>]*)>\s*)/");
    
    string line;
    int line_number = 0;
    bool success = true;

    while (getline(input, line)) {
        line_number++;
        smatch match;

        if (regex_search(line, match, include_local)) {
            path include_path = match[1].str();
            path current_dir = current_file.parent_path();
            path full_path = current_dir / include_path;

            if (!filesystem::exists(full_path)) {
                bool found = false;

                for (const auto &dir : include_dirs) {
                    full_path = dir / include_path;
                    if (filesystem::exists(full_path)) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    cout << "unknown include file " << include_path.string() 
                         << " at file " << current_file.string() 
                         << " at line " << line_number << endl;
                    success = false;
                    break;
                }
            }

            if (!ProcessInclude(full_path, output, include_dirs, current_file, line_number)) {
                success = false;
                break;
            }
        } else if (regex_search(line, match, include_global)) {
            path include_path = match[1].str();
            bool found = false;
            path full_path;

            for (const auto &dir : include_dirs) {
                full_path = dir / include_path;
                if (filesystem::exists(full_path)) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                cout << "unknown include file " << include_path.string() 
                     << " at file " << current_file.string() 
                     << " at line " << line_number << endl;
                success = false;
                break;
            }

            if (!ProcessInclude(full_path, output, include_dirs, current_file, line_number)) {
                success = false;
                break;
            }
        } else {
            output << line << endl;
        }
    }

    return success;
}

bool Preprocess(const path& input_file, const path& output_file,
                const vector<path>& include_dirs) {
    ifstream input(input_file);
    if (!input.is_open()) {
        cout << "Ошибка: Не удалось открыть входной файл: " << input_file.string() << endl;
        return false;
    }

    ofstream output(output_file);
    if (!output.is_open()) {
        cout << "Ошибка: Не удалось открыть выходной файл: " << output_file.string() << endl;
        return false;
    }

    return ProcessInclude(input_file, output, include_dirs);
}

string GetFileContents(const string& file) {
    ifstream stream(file);
    return {istreambuf_iterator<char>(stream), istreambuf_iterator<char>()};
}

void Test() {
    error_code err;
    filesystem::remove_all("sources"_p, err);
    filesystem::create_directories("sources"_p / "include2"_p / "lib"_p, err);
    filesystem::create_directories("sources"_p / "include1"_p, err);
    filesystem::create_directories("sources"_p / "dir1"_p / "subdir"_p, err);

    {
        ofstream file("sources/a.cpp");
        file << "// this comment before include\n"
                "#include \"dir1/b.h\"\n"
                "// text between b.h and c.h\n"
                "#include \"dir1/d.h\"\n"
                "\n"
                "int SayHello() {\n"
                "    cout << \"hello, world!\" << endl;\n"
                "#   include<dummy.txt>\n"
                "}\n"s;
    }
    {
        ofstream file("sources/dir1/b.h");
        file << "// text from b.h before include\n"
                "#include \"subdir/c.h\"\n"
                "// text from b.h after include"s;
    }
    {
        ofstream file("sources/dir1/subdir/c.h");
        file << "// text from c.h before include\n"
                "#include <std1.h>\n"
                "// text from c.h after include\n"s;
    }
    {
        ofstream file("sources/dir1/d.h");
        file << "// text from d.h before include\n"
                "#include \"lib/std2.h\"\n"
                "// text from d.h after include\n"s;
    }
    {
        ofstream file("sources/include1/std1.h");
        file << "// std1\n"s;
    }
    {
        ofstream file("sources/include2/lib/std2.h");
        file << "// std2\n"s;
    }

    assert(!Preprocess("sources"_p / "a.cpp"_p, "sources"_p / "a.in"_p,
                       {"sources"_p / "include1"_p, "sources"_p / "include2"_p}));

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

    assert(GetFileContents("sources/a.in"s) == test_out.str());
}

int main() {
    Test();
}
