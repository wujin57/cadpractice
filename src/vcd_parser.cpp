#include "vcd_parser.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace APBSystem {

VcdParser::VcdParser() {}

bool VcdParser::parse_file(const std::string& filename,
                           VarDefinitionCallback var_def_cb,
                           TimestampCallback time_cb,
                           ValueChangeCallback val_change_cb,
                           EndDefinitionsCallback end_def_cb) {
    // Memory-map the file
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1) {
        std::cerr << "Error: cannot open " << filename << "\n";
        return false;
    }
    struct stat sb{};
    if (fstat(fd, &sb) == -1) {
        close(fd);
        return false;
    }
    const std::size_t size = sb.st_size;
    if (size == 0) {
        close(fd);
        return true;
    }
    char* file = static_cast<char*>(mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
    close(fd);
    if (file == MAP_FAILED)
        return false;

    // Main parsing loop
    const char* ptr = file;
    const char* const end_ptr = file + size;
    std::string current_scope;

    while (ptr < end_ptr) {
        // Skip leading whitespace / EOL
        while (ptr < end_ptr && (*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n'))
            ++ptr;
        if (ptr >= end_ptr)
            break;

        // Find the end of the current line
        const char* line_start = ptr;
        const char* line_end = ptr;
        while (line_end < end_ptr && *line_end != '\n' && *line_end != '\r')
            ++line_end;
        ptr = line_end;  // Go to EOL, next loop will skip it

        // --- $keyword ---
        if (*line_start == '$') {
            const char* p = line_start + 1;
            const char* keyword_start = p;
            while (p < line_end && *p != ' ' && *p != '\t')
                ++p;
            std::string keyword(keyword_start, p - keyword_start);

            if (keyword == "var") {
                const char* type = p;
                while (type < line_end && (*type == ' ' || *type == '\t'))
                    ++type;
                const char* type_end = type;
                while (type_end < line_end && *type_end != ' ' && *type_end != '\t')
                    ++type_end;
                const char* width = type_end;
                while (width < line_end && (*width == ' ' || *width == '\t'))
                    ++width;
                const char* width_end = width;
                while (width_end < line_end && *width_end != ' ' && *width_end != '\t')
                    ++width_end;
                const char* id = width_end;
                while (id < line_end && (*id == ' ' || *id == '\t'))
                    ++id;
                const char* id_end = id;
                while (id_end < line_end && *id_end != ' ' && *id_end != '\t')
                    ++id_end;
                const char* name = id_end;
                while (name < line_end && (*name == ' ' || *name == '\t'))
                    ++name;
                const char* name_end = name;
                while (name_end < line_end && *name_end != ' ' && *name_end != '\t' && *name_end != '$')
                    ++name_end;

                std::string full_name = current_scope.empty() ? std::string(name, name_end - name) : current_scope + "." + std::string(name, name_end - name);
                if (var_def_cb)
                    var_def_cb(std::string(id, id_end - id), std::string(type, type_end - type), std::atoi(std::string(width, width_end - width).c_str()), full_name);

            } else if (keyword == "scope") {
                const char* name = p;
                while (name < line_end && (*name == ' ' || *name == '\t'))
                    ++name;
                const char* type = name;
                while (type < line_end && *type != ' ' && *type != '\t')
                    ++type;  // skip "module"
                const char* mod_name = type;
                while (mod_name < line_end && (*mod_name == ' ' || *mod_name == '\t'))
                    ++mod_name;
                const char* name_end = mod_name;
                while (name_end < line_end && *name_end != ' ' && *name_end != '\t' && *name_end != '$')
                    ++name_end;
                if (!current_scope.empty())
                    current_scope += ".";
                current_scope.append(mod_name, name_end - mod_name);

            } else if (keyword == "upscope") {
                std::size_t pos = current_scope.find_last_of('.');
                if (pos == std::string::npos)
                    current_scope.clear();
                else
                    current_scope.erase(pos);

            } else if (keyword == "enddefinitions") {
                if (end_def_cb)
                    end_def_cb();
            }
            continue;
        }

        // --- #timestamp ---
        if (*line_start == '#') {
            if (time_cb)
                time_cb(std::strtoull(line_start + 1, nullptr, 10));
            continue;
        }

        // --- value-change line ---
        const char* val_begin = line_start;
        const char* val_end = line_end;
        while (val_end > val_begin && (*(val_end - 1) == ' ' || *(val_end - 1) == '\t'))
            --val_end;
        if (val_end > val_begin && *(val_end - 1) == ' ')
            --val_end;
        if (val_end <= val_begin)
            continue;

        char id_char = *(val_end - 1);
        const char* value_ptr = val_begin;
        std::size_t value_len = val_end - val_begin;

        // Handle single-bit format like "0#" or "1%" where value and ID are adjacent
        if (value_len == 0 && (id_char >= '!' && id_char <= '~')) {
            value_len = 1;
        }

        if (val_change_cb)
            val_change_cb(id_char, value_ptr, value_len);
    }

    munmap(file, size);
    return true;
}

}  // namespace APBSystem