#include <iostream>
#include "parser.hpp"
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <vcd_file>\n";
        return 1;
    }
    parse_vcd_file(argv[1]);
    return 0;
}