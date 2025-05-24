// vcd_parser.cpp
#include "vcd_parser.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>   // For atoi
#include <cstring>   // For strcmp, strlen, strchr
#include <iostream>  // For error reporting

namespace APBSystem {

VcdParser::VcdParser() {}

// Adapted from your existing parser.cpp
int VcdParser::tokenize_line(char* line_buffer, char** argv) {
    int argc = 0;
    char* current_pos = line_buffer;
    char* token_start = nullptr;

    while (*current_pos != '\0' && argc < 63) {  // Leave space for NULL terminator for argv
        // Skip leading whitespace
        while (*current_pos == ' ' || *current_pos == '\t' || *current_pos == '\n' || *current_pos == '\r') {
            current_pos++;
        }

        if (*current_pos == '\0')
            break;  // End of line

        token_start = current_pos;

        // Find end of token
        while (*current_pos != ' ' && *current_pos != '\t' && *current_pos != '\n' && *current_pos != '\r' && *current_pos != '\0') {
            current_pos++;
        }

        argv[argc++] = token_start;

        if (*current_pos != '\0') {
            *current_pos = '\0';  // Terminate token
            current_pos++;
        }
    }
    argv[argc] = nullptr;  // Null-terminate argv
    return argc;
}

bool VcdParser::parse_value_change_line(const char* line, std::string& out_value, std::string& out_id) {
    // VCD value change can be:
    // 1. <value><id_code> (e.g., 1# or b0101!)
    // 2. <value> <id_code> (e.g., b0101 !  -- less common for EDA tools but possible)
    // We'll assume no space for now, as per typical VCD. If space is present, tokenize_line handles it.

    const char* p = line;
    const char* id_start = nullptr;

    // Skip scalar or binary prefix
    if (*p == 'b' || *p == 'B') {
        p++;  // Skip 'b'
        while (*p == '0' || *p == '1' || *p == 'x' || *p == 'X' || *p == 'z' || *p == 'Z') {
            p++;
        }
        id_start = p;
    } else if (*p == 'r' || *p == 'R') {  // Real numbers (less common for APB signals)
        p++;                              // Skip 'r'
        while ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '+' || *p == '-') {
            p++;
        }
        id_start = p;
    } else if (*p >= '0' && *p <= '9') {                                     // Could be start of decimal or scalar '0' or '1'
                                                                             // If it's just '0' or '1' followed by ID
        if ((*(p + 1) != '\0' && !isdigit(*(p + 1))) || *(p + 1) == '\0') {  // Single digit scalar
            p++;
            id_start = p;
        } else {  // Multi-digit number (not expected for APB control signals, maybe data if not binary)
            while (*p >= '0' && *p <= '9')
                p++;
            id_start = p;
        }
    } else if (*p == 'x' || *p == 'X' || *p == 'z' || *p == 'Z') {  // Scalar x or z
        p++;
        id_start = p;
    } else {
        return false;  // Not a recognized value start
    }

    if (id_start && *id_start != '\0') {
        out_value.assign(line, id_start - line);
        out_id.assign(id_start);
        // Trim trailing whitespace from id (though usually not present)
        size_t endpos = out_id.find_last_not_of(" \t\n\r");
        if (std::string::npos != endpos) {
            out_id = out_id.substr(0, endpos + 1);
        }
        return !out_id.empty();
    }
    return false;
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
        std::cerr << "Error: Could not get file size for " << filename << std::endl;
        close(fd);
        return false;
    }
    size_t filesize = sb.st_size;
    if (filesize == 0) {
        close(fd);
        return true;  // Empty file, parsed successfully.
    }

    char* mapped_data = static_cast<char*>(mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0));
    if (mapped_data == MAP_FAILED) {
        std::cerr << "Error: mmap failed for " << filename << std::endl;
        close(fd);
        return false;
    }
    close(fd);  // fd no longer needed after mmap

    const char* p = mapped_data;
    const char* end = mapped_data + filesize;
    std::string current_line_str;

    while (p < end) {
        const char* line_start = p;
        while (p < end && *p != '\n') {
            p++;
        }
        current_line_str.assign(line_start, p - line_start);

        // Skip empty lines or lines with only whitespace (after trimming)
        size_t first_char = current_line_str.find_first_not_of(" \t\r");
        if (first_char == std::string::npos) {
            if (p < end && *p == '\n')
                p++;
            continue;
        }
        // Use a mutable buffer for tokenize_line
        char line_buffer[4096];  // Reasonably large buffer for a single VCD line
        strncpy(line_buffer, current_line_str.c_str(), sizeof(line_buffer) - 1);
        line_buffer[sizeof(line_buffer) - 1] = '\0';

        if (line_buffer[0] == '$') {  // VCD Keyword
            int argc = tokenize_line(line_buffer, targv_);
            if (argc > 0) {
                if (strcmp(targv_[0], "$var") == 0) {
                    if (argc >= 5) {  // $var type width id name [vector_range]
                        // targv_[1]=type, targv_[2]=width, targv_[3]=id, targv_[4]=name
                        var_def_cb(targv_[3], targv_[1], std::atoi(targv_[2]), targv_[4]);
                    }
                } else if (strcmp(targv_[0], "$enddefinitions") == 0) {
                    if (end_def_cb)
                        end_def_cb();
                } else if (strcmp(targv_[0], "$dumpvars") == 0) {
                    // Dumpvars section starts, initial values follow until $end
                } else if (strcmp(targv_[0], "$end") == 0) {
                    // This is the $end for $dumpvars typically
                    // Need a state to know if we are in dumpvars section
                    if (end_dumpvars_cb)
                        end_dumpvars_cb();
                }
                // Handle other keywords: $scope, $upscope, $date, $version, $timescale as needed
            }
        } else if (line_buffer[0] == '#') {  // Timestamp
            if (time_cb)
                time_cb(std::atoi(line_buffer + 1));
        } else {  // Value change
            std::string val_str, id_str;
            // The line_buffer might already be tokenized if there were spaces.
            // If no spaces, parse_value_change_line is more direct.
            // Let's try direct parsing from original non-tokenized line_str first.
            if (parse_value_change_line(current_line_str.c_str(), val_str, id_str)) {
                if (val_change_cb)
                    val_change_cb(id_str, val_str);
            } else {
                // Fallback to tokenized version if direct parse fails and line might have spaces
                // This part needs refinement if VCD format has <value> <space> <id>
                int argc = tokenize_line(line_buffer, targv_);
                if (argc == 2) {  // value id
                    if (val_change_cb)
                        val_change_cb(targv_[1], targv_[0]);
                } else if (argc == 1 && current_line_str.length() > 1) {
                    // This case could be "b0101!" with no space. Already handled by parse_value_change_line.
                    // If parse_value_change_line failed, this is likely an error or unhandled format.
                }
            }
        }

        if (p < end && *p == '\n')
            p++;  // Move to next line
    }

    munmap(mapped_data, filesize);
    return true;
}

}  // namespace APBSystem