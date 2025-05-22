#include "signal_table.hpp"
#include <cstdlib>  // For std::atoi
#include <cstring>  // For strncmp etc.
#include <iostream>
#include <string>
#include <vector>           // For std::vector with char buffer
#include "transaction.hpp"  // For SignalState, current_time

// Helper to parse binary string (handles '0' and '1' only)
// VCD can have 'x' and 'z', this parser is simplified.
// The contest PDF Q&A Q6 mentions 'X' can appear.
uint32_t parse_binary_val(const char* s, int& len) {
    uint32_t result = 0;
    len = 0;
    while (*s == '0' || *s == '1') {  // Original was *s==1, should be *s=='1'
        result = (result << 1) | (*s - '0');
        ++s;
        ++len;
    }
    // if (*s == 'x' || *s == 'X' || *s == 'z' || *s == 'Z') {
    //     // Handle X or Z: For APB, if an address/data bit is X, it's usually an error state.
    //     // For now, this parser treats them as end of binary string.
    //     // A more robust parser would set a flag or return a special value.
    // }
    return result;
}

// Using a simple direct mapping for VCD identifiers to an integer.
// This can be fragile if identifiers are complex. A std::map is more robust.
constexpr int MAX_VCD_ID_CODE = 10000;  // Estimate max possible encoded IDs
struct SignalMapping {
    enum class SignalType { PADDR,
                            PWDATA,
                            PRDATA,
                            PWRITE,
                            PSEL,
                            PENABLE,
                            PREADY,
                            PSLVERR,
                            PRESETN,
                            PCLK,
                            OTHER };
    SignalType type;
    int bit_width = 1;  // For multi-bit signals
};
SignalMapping g_signal_map[MAX_VCD_ID_CODE];  // Crude map; consider std::map<int, SignalMapping>

// VCD identifiers are printable ASCII chars from ! (33) to ~ (126)
int encode_vcd_id(const std::string& id_str) {
    if (id_str.empty() || id_str.length() > 4) {  // Limit length to avoid huge codes / overflow
        return -1;                                // Invalid or too long
    }
    int code = 0;
    for (char c : id_str) {
        if (c < 33 || c > 126)
            return -1;                // Invalid char
        code = code * 94 + (c - 33);  // 94 printable characters
    }
    return code % MAX_VCD_ID_CODE;  // Modulo to fit in array, introduces collision risk!
                                    // A std::map<std::string, SignalMapping> is much better here.
}

// --- These should be part of the global state in transaction.cpp ---
// static SignalState signal_state; // Defined in transaction.cpp
// static SignalState prev_state;   // Defined in transaction.cpp
// static int current_time;         // Defined in transaction.cpp

void register_signal(char** tokens, int count) {
    // $var <type> <width> <id_code> <reference> $end
    if (count >= 5) {  // Need at least $var, type, width, id, name
        // std::string var_type_str = tokens[1]; // e.g., wire, reg
        std::string width_str = tokens[2];
        std::string id_str = tokens[3];
        std::string name_str = tokens[4];
        // Optional: tokens[5] could be bit select like [7:0] or $end

        int width = std::atoi(width_str.c_str());
        int encoded_id = encode_vcd_id(id_str);

        if (encoded_id != -1) {
            SignalMapping::SignalType sig_type = SignalMapping::SignalType::OTHER;
            if (name_str == "PADDR")
                sig_type = SignalMapping::SignalType::PADDR;
            else if (name_str == "PWDATA")
                sig_type = SignalMapping::SignalType::PWDATA;
            else if (name_str == "PRDATA")
                sig_type = SignalMapping::SignalType::PRDATA;
            else if (name_str == "PWRITE")
                sig_type = SignalMapping::SignalType::PWRITE;
            else if (name_str == "PSEL")
                sig_type = SignalMapping::SignalType::PSEL;  // Single PSEL
            // TODO: Handle multiple PSELs if they are named e.g. PSEL_0, PSEL_S0 etc.
            // The name_str would need more sophisticated parsing to identify completer index.
            else if (name_str == "PENABLE")
                sig_type = SignalMapping::SignalType::PENABLE;
            else if (name_str == "PREADY")
                sig_type = SignalMapping::SignalType::PREADY;
            else if (name_str == "PSLVERR")
                sig_type = SignalMapping::SignalType::PSLVERR;
            else if (name_str == "PRESETN")
                sig_type = SignalMapping::SignalType::PRESETN;
            else if (name_str == "PCLK")
                sig_type = SignalMapping::SignalType::PCLK;

            g_signal_map[encoded_id] = {sig_type, width};
            // std::cout << "Registered VCD ID: " << id_str << " (code " << encoded_id << ") as "
            //           << name_str << " with width " << width << std::endl;
        } else {
            // std::cerr << "Warning: Could not encode VCD ID: " << id_str << std::endl;
        }
    }
}

void handle_signal_event(const char* line) {
    if (line == nullptr || line[0] == '\0')
        return;

    if (line[0] == '#') {
        current_time = std::atoi(line + 1);
        // std::cout << "[Time Update: " << current_time << "]" << std::endl; // Debug
        return;
    }

    // Format: <value><identifier_code> for 1-bit scalar, or b<value> <identifier_code> for vector
    // Or, from Q&A, even 1-bit might be b0! or b1!
    // Example line: "b1 PSEL_ID" or "0 PWRITE_ID" or "b0101010 PADDR_ID"

    const char* p = line;
    uint32_t val = 0;
    bool is_binary_vector = false;
    int val_len = 0;  // length of the binary string if 'b' prefix

    if (*p == 'b' || *p == 'B') {
        is_binary_vector = true;
        p++;  // Skip 'b'
        val = parse_binary_val(p, val_len);
        p += val_len;                     // Move pointer past the binary digits
    } else if (*p == '0' || *p == '1') {  // Scalar 0 or 1
        val = (*p - '0');
        p++;
        val_len = 1;
    } else if (*p == 'x' || *p == 'X' || *p == 'z' || *p == 'Z') {
        // Handle X or Z. For APB logic signals, X might mean an issue.
        // For now, we'll treat it as a value that can't be easily converted to uint32_t for arithmetic.
        // You might need a way to flag signals as 'X' in SignalState.
        // For simplicity, let's assume it means 0 for now, but this is often incorrect.
        // TODO: Robust X/Z handling. The contest says 'X' can appear.
        val = 0;  // Placeholder for X/Z
        // std::cout << "Warning: X/Z value encountered for signal. Treating as 0. Line: " << line << std::endl;
        p++;
        val_len = 1;
    } else {
        // std::cerr << "Warning: Unknown value format in VCD line: " << line << std::endl;
        return;
    }

    // Skip whitespace between value and identifier
    while (*p == ' ' || *p == '\t') {
        p++;
    }

    std::string id_str = p;  // The rest of the string is the VCD identifier
    if (id_str.empty()) {
        // std::cerr << "Warning: Missing identifier in VCD line: " << line << std::endl;
        return;
    }

    int encoded_id = encode_vcd_id(id_str);

    if (encoded_id != -1 && encoded_id < MAX_VCD_ID_CODE) {
        SignalMapping mapping = g_signal_map[encoded_id];
        // std::cout << "Event: ID " << id_str << " (code " << encoded_id << "), Val " << val << (is_binary_vector ? " (vector)" : " (scalar)") << std::endl;

        switch (mapping.type) {
            case SignalMapping::SignalType::PADDR:
                signal_state.paddr = val;
                break;
            case SignalMapping::SignalType::PWDATA:
                signal_state.pwdata = val;
                break;
            case SignalMapping::SignalType::PRDATA:
                signal_state.prdata = val;
                break;
            case SignalMapping::SignalType::PWRITE:
                signal_state.pwrite = (val != 0);
                break;
            case SignalMapping::SignalType::PSEL:
                signal_state.psel = (val != 0);
                break;
            // TODO: If multiple PSELs, this needs to map to the correct SignalState member
            // e.g. if(name_str from register_signal was "PSEL_SLAVE0") signal_state.psel_slave0 = (val != 0);
            case SignalMapping::SignalType::PENABLE:
                signal_state.penable = (val != 0);
                break;
            case SignalMapping::SignalType::PREADY:
                signal_state.pready = (val != 0);
                break;
            case SignalMapping::SignalType::PSLVERR:
                signal_state.pslverr = (val != 0);
                break;
            case SignalMapping::SignalType::PRESETN:
                signal_state.presetn = (val != 0);
                break;
            case SignalMapping::SignalType::PCLK:
                signal_state.pclk = (val != 0);
                break;
            case SignalMapping::SignalType::OTHER: /* ignore or log */
                break;
        }
    } else {
        // std::cerr << "Warning: Unregistered or invalid VCD ID '" << id_str << "' in line: " << line << std::endl;
    }
}