// vcd_parser.hpp
#pragma once
#include <functional>
#include <string>
#include <vector>  // For targv_

namespace APBSystem {

class VcdParser {
   public:
    // Callback types
    using VarDefinitionCallback = std::function<void(const std::string& id, const std::string& type_str, int width, const std::string& name)>;
    using TimestampCallback = std::function<void(int time)>;
    using ValueChangeCallback =
        std::function<void(char id_char,
                           const char* value_begin,
                           std::size_t value_len)>;
    using EndDefinitionsCallback = std::function<void()>;  // Callback for $enddefinitions
    using EndDumpvarsCallback = std::function<void()>;     // Callback for $dumpvars $end

    VcdParser();
    bool parse_file(const std::string& filename,
                    VarDefinitionCallback,
                    TimestampCallback,
                    ValueChangeCallback,
                    EndDefinitionsCallback);
};

}  // namespace APBSystem