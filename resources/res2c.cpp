#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <cctype>
#include <bitset>

#include <docopt/docopt.h>
#include <fmt/core.h>

static const char USAGE[] =
R"(res2c. Convert resource files to code.

    Usage:
      res2c [--binary|--shaders] <name>...
      res2c (-h | --help)
      res2c --version

    Options:
      -h --help     Show this screen.
      --version     Show version.
)";


//! Convert shader file's contents into string literals.
//!
//! Single raw literal has max size restriction that we can't use one raw
//! string for complex shader code. Therefore, we convert each line into
//! one string literal to avoid that restrication in MSVC.
void appendShaderContent(std::string shaderFilePath, std::fstream& outfile)
{
    // We use filename as the string variable name used in source code.
    std::string filename = shaderFilePath.substr(shaderFilePath.find_last_of('/') + 1);
    std::replace(filename.begin(), filename.end(), '.', '_');

    std::fstream infile(shaderFilePath);
    if (!infile.is_open()) {
        throw std::runtime_error("Failed to open file " + shaderFilePath);
    }

    std::string line;
    
    outfile << "const char* " << filename << " = ";
    
    while (std::getline(infile, line)) {
        outfile << "\"" << line << "\\n\"\n";
    }

    outfile << ";\n\n";
}


int main(int argc, char* argv[])
{
    constexpr bool showHelpWhenRequest = true;
    std::map<std::string, docopt::value> args
        = docopt::docopt(USAGE, { argv + 1, argv + argc }
        , showHelpWhenRequest, "res2c");

    if (args["--shaders"].asBool()) {
        std::fstream outfile("shader_resources.h", std::ios::out);
        if (!outfile.is_open()) {
            return 1;
        }

        outfile << "#pragma once\n\n";
        for (auto path : args["<name>"].asStringList()) {
            appendShaderContent(path, outfile);
        }
    } else if (args["--binary"].asBool()) {
        std::fstream outHeaderFile("resources.h", std::ios::out);
        outHeaderFile << "// Auto generated by res2c.\n\n";
        outHeaderFile << "#pragma once\n#include <stdint.h>\n\n";
        
        std::fstream outSourceFile("resources.cpp", std::ios::out);
        outSourceFile << "// Auto generated by res2c.\n\n";
        outSourceFile << "#pragma once\n#include <stdint.h>\n\n";

        for (const std::string& path : args["<name>"].asStringList()) {
            std::string filename = path.substr(path.find_last_of('/') + 1);
            std::transform(filename.begin(), filename.end(), filename.begin(), [](char v) {
                if (v == '.' || v == '-') {
                    return '_';
                }
                return static_cast<char>(std::tolower(v));
            });

            outHeaderFile << "extern uint8_t " << filename << "[];\n\n";
            outSourceFile << "uint8_t " << filename << "[] = {\n\t";
            
            std::ifstream infile(path, std::ios::in | std::ios::binary);
            if (!infile.is_open()) {
                throw std::runtime_error("Failed to open file " + path);
            }
            
            infile >> std::noskipws; // read every byte including white space

            uint8_t byte;
            int count = 0;
            while (infile >> byte) {
                outSourceFile << "0x" << std::hex << std::setfill('0') << std::setw(2) << std::uppercase << static_cast<int>(byte) << ", ";

                if (++count == 16) {
                    count = 0;
                    outSourceFile << "\n\t";
                }
            }

            outSourceFile << "};\n\n";

            outHeaderFile << "extern uint32_t " << filename << "_size;\n\n";
            outSourceFile << "uint32_t " << filename << "_size = sizeof(" << filename  << ");\n\n";
        }
    }

    return 0;
}