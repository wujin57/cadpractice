#include "vcd_parser.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>  // For std::atoi, std::stoull
#include <cstring>  // For strcmp, strncpy
#include <iostream>
#include <string>  // For std::string operations
#include <vector>  // For line buffer storage if needed, though mmap is char*

namespace APBSystem {

VcdParser::VcdParser() {
    // Initialize any members if necessary, e.g., m_timescale_value, m_timescale_unit
}

// 沿用您提供的 tokenize_line
int VcdParser::tokenize_line(char* line_buffer, char** argv) {
    int argc = 0;
    char* p = line_buffer;
    while (*p && argc < 63) {  // Max 63 arguments, argv[63] is for nullptr
        while (*p && isspace(static_cast<unsigned char>(*p)))
            *p++ = '\0';  // Nullify leading spaces
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && !isspace(static_cast<unsigned char>(*p)))
            p++;  // Move to end of token
        if (!*p)
            break;  // End of string
        // *p = '\0'; // Nullify space after token (這行通常是需要的，以便 argv[i] 是以null結尾的字串)
        // p++;       // 且 p 指向下一個token的開始或空格
        // 但如果VCD的token之間有多個空格，上面的 while isspace 會處理
        // 這裡的處理方式取決於您希望 tokenize_line 如何精確地分割
        // 如果 $var type width id name $end 這樣的格式，每個都是一個token
        // 則在非空格後遇到空格，應將該空格變為 '\0'，然後 p++
    }
    argv[argc] = nullptr;
    return argc;
}

bool VcdParser::parse_value_change_line(const char* line_ptr, std::string& out_value, std::string& out_id) {
    out_value.clear();
    out_id.clear();

    const char* p = line_ptr;
    // Skip leading whitespace
    while (*p && isspace(static_cast<unsigned char>(*p))) {
        p++;
    }
    if (!*p)
        return false;  // Empty line or all whitespace

    const char* value_start = p;
    const char* id_start = nullptr;

    const char* line_end = p + strlen(p);
    const char* current_char = line_end - 1;  // Start from last char of the effective line

    if (current_char < value_start)
        return false;  // Line too short or empty after trim

    // Find the start of the ID (usually the last character for single-char IDs)
    // VCD IDs are single characters from '!' (33) to '~' (126)
    if (*current_char >= 33 && *current_char <= 126) {
        id_start = current_char;
        // In VCD, an ID is typically a single character.
        // Some tools *might* produce multi-character IDs, but it's not standard for $dumpvars value changes.
        // We assume single character ID for simplicity here matching typical VCD.
        // If multi-character IDs are possible AND defined in $var, this logic needs adjustment.
        // For now, assume ID is the last char if it's in range.
    } else {
        return false;  // Last char is not a valid ID char
    }

    // Value is everything before the ID
    if (id_start > value_start) {  // Check if there's anything before the ID
        out_value.assign(value_start, id_start - value_start);
        // Trim trailing space from value if any (e.g. "b0101 #") - though less common
        if (!out_value.empty() && isspace(static_cast<unsigned char>(out_value.back()))) {
            out_value.pop_back();
        }
    } else if (id_start == value_start) {  // Value is empty, only ID, e.g. "#" - not valid for value change
                                           // This case indicates the line might ONLY be an ID, or parsing is confused.
                                           // A valid value change line requires a value part.
                                           // However, VCD might have lines like "x#" or "0%".
                                           // If the *first* char is an ID and it's also the last, it might be a scalar value.
                                           // Example "0#" value is "0", id is "#"
                                           // Example "x%" value is "x", id is "%"
                                           // This case should be value_start IS the value, and id_start is the ID.
                                           // Example: "0#" -> value_start points to '0', id_start points to '#'
                                           // If line is "0#", then id_start is p[1], value_start is p[0]. value is p[0] to p[0] (1 char)
        out_value.assign(value_start, 1);
    } else {
        return false;  // Should not happen if id_start was found
    }

    out_id.assign(id_start, 1);  // Assuming single character ID

    return !out_id.empty();  // Value can be empty if signal type allows (e.g. events), but ID must exist.
                             // For typical APB signals, value won't be empty unless it's 'x' or 'z'.
}

bool VcdParser::parse_file(const std::string& filename,
                           VarDefinitionCallback var_def_cb,
                           TimestampCallback time_cb,
                           ValueChangeCallback val_change_cb,
                           EndDefinitionsCallback end_def_cb,
                           EndDumpvarsCallback end_dumpvars_cb) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Error: Could not open VCD file: " << filename << std::endl;
        return false;
    }
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        close(fd);
        return false;
    }
    size_t filesize = sb.st_size;
    if (filesize == 0) {
        std::cout << "Warning: VCD file is empty: " << filename << std::endl;
        close(fd);
        return true;  // Technically not an error, just no data.
    }

    char* mapped_file_content = static_cast<char*>(mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0));
    if (mapped_file_content == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return false;
    }
    close(fd);  // fd no longer needed after mmap

    const char* current_pos = mapped_file_content;
    const char* end_of_file = mapped_file_content + filesize;

    std::vector<char> line_buffer_vec(2048);  // Increased buffer size for long lines
    bool in_dumpvars_section = false;
    std::string current_scope;  // To build hierarchical names

    while (current_pos < end_of_file) {
        const char* line_start_ptr = current_pos;
        // Find the end of the current line
        while (current_pos < end_of_file && *current_pos != '\n' && *current_pos != '\r') {
            current_pos++;
        }

        std::string line_str(line_start_ptr, current_pos - line_start_ptr);

        // Advance current_pos past the EOL characters
        while (current_pos < end_of_file && (*current_pos == '\n' || *current_pos == '\r')) {
            current_pos++;
        }

        // Trim leading whitespace for keyword/timestamp check
        size_t first_char_idx = line_str.find_first_not_of(" \t");
        if (first_char_idx == std::string::npos)
            continue;  // Skip empty or all-whitespace line

        char first_char_of_line = line_str[first_char_idx];
        const char* effective_line_start = line_str.c_str() + first_char_idx;

        if (first_char_of_line == '$') {
            // Copy to mutable buffer for tokenization using existing targv_ approach
            strncpy(line_buffer_vec.data(), effective_line_start, line_buffer_vec.size() - 1);
            line_buffer_vec[line_buffer_vec.size() - 1] = '\0';  // Ensure null termination

            int argc = tokenize_line(line_buffer_vec.data(), targv_);

            if (argc > 0) {
                std::string keyword = targv_[0];  // Keyword is $command
                if (keyword == "$var" && argc >= 5) {
                    // $var <type_str> <width_str> <id_code_str> <name_suffix_str> ... ($end might be a token)
                    // Ensure name_suffix_str does not include [xx:yy] if VcdParser should strip it
                    // Or SignalManager handles it. Let's assume VcdParser provides it as is.
                    std::string signal_name_suffix = targv_[4];
                    std::string full_hierarchical_name = current_scope.empty() ? signal_name_suffix : current_scope + "." + signal_name_suffix;

                    // Remove potential trailing $end from the name if tokenize_line includes it
                    if (argc > 5 && strcmp(targv_[argc - 1], "$end") == 0) {
                        // This assumes the name is a single token. If name has spaces, tokenize_line needs care.
                        // VCD standard: $var type size identifier reference $end
                        // reference might not have spaces.
                    }

                    var_def_cb(targv_[3], targv_[1], std::atoi(targv_[2]), full_hierarchical_name);
                } else if (keyword == "$scope" && argc >= 3) {
                    if (!current_scope.empty())
                        current_scope += ".";
                    current_scope += targv_[2];  // $scope module <name> $end
                } else if (keyword == "$upscope" && argc >= 2) {
                    size_t last_dot = current_scope.find_last_of('.');
                    if (last_dot != std::string::npos) {
                        current_scope = current_scope.substr(0, last_dot);
                    } else {
                        current_scope.clear();
                    }
                } else if (keyword == "$timescale") {
                    // TODO: Parse timescale if needed by VcdParser itself
                    // For example: $timescale 1 ps $end
                    // if (argc >= 3) {
                    //     m_timescale_value = std::stoull(targv_[1]);
                    //     m_timescale_unit = targv_[2];
                    // }
                } else if (keyword == "$enddefinitions") {
                    if (end_def_cb)
                        end_def_cb();
                } else if (keyword == "$dumpvars") {
                    in_dumpvars_section = true;  // Initial values follow, then #0
                } else if (keyword == "$end") {  // Could be $end for $dumpvars or other sections
                    if (in_dumpvars_section) {   // Assume this $end is for $dumpvars
                        // This might be incorrect if $end is for $scope etc.
                        // A better way: check what section $end belongs to.
                        // For VCD, $dumpvars content is followed by #timestamp.
                        // $end for $dumpvars is usually the LAST $end before #time.
                        // The logic here assumes $end after $dumpvars tokens implies end of initial values.
                        // The problem description implies we care about time-series after dumpvars.
                        // Let's assume `end_dumpvars_cb` is called when actual timed simulation starts (e.g. at first #0)
                        // Or simply: when $dumpvars is seen, all subsequent value changes until first #time are initials.
                    }
                }
            }
        } else if (first_char_of_line == '#') {
            if (in_dumpvars_section) {  // First timestamp after $dumpvars effectively ends initial value section
                in_dumpvars_section = false;
                if (end_dumpvars_cb)
                    end_dumpvars_cb();
            }
            // Timestamp line, skip '#' and parse number
            try {
                uint64_t time_val = std::stoull(effective_line_start + 1);
                // m_last_parsed_timestamp = time_val;
                if (time_cb)
                    time_cb(time_val);
            } catch (const std::exception& e) {
                // std::cerr << "Warning: Could not parse timestamp: " << effective_line_start << " (" << e.what() << ")" << std::endl;
            }
        } else {                        // Assumed to be a value change line
            if (in_dumpvars_section) {  // These are initial values
                                        // These are applied at "time 0" conceptually, before first #timestamp
            }

            std::string val_str, id_str;
            // Use the refined parse_value_change_line (assuming it's part of this class or accessible)
            if (parse_value_change_line(effective_line_start, val_str, id_str)) {
                if (val_change_cb)
                    val_change_cb(id_str, val_str);
            } else {
                // It's possible that lines within $dumpvars (like "b0 %") are parsed here.
                // Or general comments if not starting with $.
                // Only log warning if not in dumpvars and parsing fails.
                // if (!in_dumpvars_section && !line_str.empty()) { // Avoid warning for empty lines after processing
                //    std::cerr << "Warning: Failed to parse as value change: " << line_str << std::endl;
                // }
            }
        }
    }

    munmap(mapped_file_content, filesize);
    return true;
}

}  // namespace APBSystem