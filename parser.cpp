#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include <parser.hpp>
#include "signal_table.hpp"
#include "transaction.hpp"  // Added this based on context, might not be in original if it was an error

const int MAX_ARGC = 64;
char* targv[MAX_ARGC];
int targc;

int parse_line(char* line, char** argv) {
    int argc = 0;
    char ch;
    while ((ch = *line) != '\0') {
        if (ch <= ' ') {
            ++line;
            continue;
        }
        argv[argc++] = line;
        while ((ch = *line) != '\0' && ch > ' ')
            ++line;
        if (*line != '\0')
            *line++ = '\0';
    }
    return argc;
}
/*void parse_vcd_file(const std::string& filename) {
    std::ifstream fin(filename);
    if (!fin) {
        std::cerr << "Can't open file " << filename << "\n"; return;
    }

    std::string linebuf;
    while (std::getline(fin, linebuf)) {
        char* line = linebuf.data();
        targc = parse_line(line, targv);
        if (targc == 0) continue;
        if (strcmp(targv[0], "$var") == 0) {
            var_definition(targv, targc);
        } else {
            handle_signal_event(line);
        }
    }
}*/
void parse_vcd_file(const std::string& filename) {
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("open");
        return;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat");
        close(fd);
        return;
    }

    size_t filesize = st.st_size;
    char* data = (char*)mmap(nullptr, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return;
    }
    close(fd);

    char* p = data;
    char* end = data + filesize;
    std::string line_str_buf;  // Renamed to avoid conflict with char* line

    while (p < end) {
        char* line_start = p;
        while (p < end && *p != '\n')
            ++p;
        // line_str_buf.assign(line_start, p); // This was incorrect, p might not be null-terminated within line_str_buf correctly
        line_str_buf.assign(line_start, p - line_start);  // Correct way to assign substring

        // Process line_str_buf which now holds a single line without newline
        // Remove \r if present
        if (!line_str_buf.empty() && line_str_buf.back() == '\r') {
            line_str_buf.pop_back();
        }

        // Need to convert line_str_buf to char* for parse_line, or rewrite parse_line
        // For now, let's assume parse_line is adapted or we use line_str_buf directly with string operations
        char* line_for_parse = &line_str_buf[0];  // Risky if line_str_buf is empty
        if (line_str_buf.empty()) {
            if (p < end && *p == '\n')
                ++p;  // Consume the newline
            continue;
        }

        targc = parse_line(line_for_parse, targv);  // parse_line modifies its input string
        if (targc == 0) {
            if (p < end && *p == '\n')
                ++p;
            continue;
        }

        if (strcmp(targv[0], "$var") == 0) {
            register_signal(targv, targc);
        } else {
            // handle_signal_event expects const char*. We passed line_for_parse which was modified.
            // It's better to pass the original line_str_buf.c_str() if handle_signal_event doesn't modify.
            // Or, if handle_signal_event is robust:
            // The VCD standard has values first then identifiers for signal changes.
            // #timestamp
            // value identifier
            // bvalue identifier
            // So, targv[0] would be the value (or #timestamp), targv[1] (if present) is the identifier.
            // This part needs to align with how handle_signal_event and check_transaction_event expect input.
            // The original parser_switch_optimized.cpp had a better line classification.

            // Based on your original parser_switch_optimized.cpp structure:
            const char* current_line_ptr = line_str_buf.c_str();
            if (current_line_ptr[0] == '$') {  // Meta
                // $var was handled above by parse_line and strcmp. Other meta ignored for now.
            } else if (current_line_ptr[0] == '#') {    // Timestamp
                handle_signal_event(current_line_ptr);  // Assumes handle_signal_event can parse #timestamp
                // check_transaction_event(); // Usually called AFTER all signal events for a given timestamp are processed
            } else if (current_line_ptr[0] == 'b' || current_line_ptr[0] == '0' || current_line_ptr[0] == '1' || current_line_ptr[0] == 'x' || current_line_ptr[0] == 'z') {  // Scalar or Vector value
                handle_signal_event(current_line_ptr);
            }
        }
        // check_transaction_event should be called after all signal changes for a given timestamp are processed.
        // This means if a VCD has multiple signal changes under one #timestamp,
        // call handle_signal_event for all of them, then call check_transaction_event once.
        // A simple way is to call it if the *next* line is a #timestamp or EOF.
        if ((p + 1 < end && *(p + 1) == '#') || (p + 1 >= end)) {
            check_transaction_event();
        }

        if (p < end && *p == '\n')
            ++p;  // Consume the newline for the current line
    }

    munmap(data, filesize);
}