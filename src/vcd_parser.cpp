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
    using ValueChangeCallback = std::function<void(const std::string& id, const std::string& value_str)>;
    using EndDefinitionsCallback = std::function<void()>;  // Callback for $enddefinitions
    using EndDumpvarsCallback = std::function<void()>;     // Callback for $dumpvars $end

    VcdParser();
    bool parse_file(const std::string& filename,
                    VarDefinitionCallback var_def_cb,
                    TimestampCallback time_cb,
                    ValueChangeCallback val_change_cb,
                    EndDefinitionsCallback end_def_cb,
                    EndDumpvarsCallback end_dumpvars_cb);

   private:
    char* targv_[64];  // Max tokens per line, adjust if needed
    int tokenize_line(char* line_buffer, char** argv);

    // Helper to parse value change lines robustly
    bool parse_value_change_line(const char* line, std::string& out_value, std::string& out_id);
};

}  // namespace APBSystem