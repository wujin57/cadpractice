#ifndef COMPLETER_INFO_HPP
#define COMPLETER_INFO_HPP

#include <string>
#include <vector>
#include <cstdint>

// 定義單一 Completer 的結構
struct Completer {
    std::string name;
    uint64_t start_addr;
    uint64_t end_addr;
};

// 為了方便管理，我們用一個函式來提供所有 Completer 的定義
// 根據 Q&A A5 的定義
inline std::vector<Completer> get_completer_definitions() {
    std::vector<Completer> completers;
    
    Completer c1 = {"Completer A", 0x10000000, 0x10000FFF};
    completers.push_back(c1);

    Completer c2 = {"Completer B", 0x10001000, 0x10001FFF};
    completers.push_back(c2);

    Completer c3 = {"Completer C", 0x10002000, 0x10002FFF};
    completers.push_back(c3);

    return completers;
}

#endif // COMPLETER_INFO_HPP

