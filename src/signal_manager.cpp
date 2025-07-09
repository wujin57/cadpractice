#include "signal_manager.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace APBSystem {

SignalManager::SignalManager() {}

VcdSignalPhysicalType SignalManager::deduce_physical_type_from_name(const std::string& hierarchical_name, const std::string& vcd_type_str) {
    if (vcd_type_str == "parameter")
        return VcdSignalPhysicalType::PARAMETER;

    std::string name_suffix;
    size_t last_dot = hierarchical_name.find_last_of('.');
    if (last_dot != std::string::npos) {
        name_suffix = hierarchical_name.substr(last_dot + 1);
    } else {
        name_suffix = hierarchical_name;
    }

    size_t bracket_pos = name_suffix.find('[');
    if (bracket_pos != std::string::npos) {
        name_suffix = name_suffix.substr(0, bracket_pos);
    }

    name_suffix.erase(0, name_suffix.find_first_not_of(" \t\n\r\f\v"));
    name_suffix.erase(name_suffix.find_last_not_of(" \t\n\r\f\v") + 1);

    if (name_suffix == "clk" || name_suffix == "pclk")
        return VcdSignalPhysicalType::PCLK;  // 兼容 pclk
    if (name_suffix == "rst_n" || name_suffix == "presetn")
        return VcdSignalPhysicalType::PRESETN;  
    if (name_suffix == "paddr")
        return VcdSignalPhysicalType::PADDR;
    if (name_suffix == "pwrite")
        return VcdSignalPhysicalType::PWRITE;
    if (name_suffix == "psel")
        return VcdSignalPhysicalType::PSEL;
    if (name_suffix == "penable")
        return VcdSignalPhysicalType::PENABLE;
    if (name_suffix == "pwdata")
        return VcdSignalPhysicalType::PWDATA;
    if (name_suffix == "prdata")
        return VcdSignalPhysicalType::PRDATA;
    if (name_suffix == "pready")
        return VcdSignalPhysicalType::PREADY;

    // std::cerr << "Warning: Unknown signal type for name suffix: " << name_suffix << " (full: " << hierarchical_name << ")" << std::endl;
    return VcdSignalPhysicalType::OTHER;
}

void SignalManager::register_signal(const std::string& vcd_id_code,
                                    const std::string& type_str,
                                    int width,
                                    const std::string& hierarchical_name) {
    if (vcd_id_code.empty()) {
        return;
    }
    VcdSignalInfo info;
    info.hierarchical_name = hierarchical_name;
    info.bit_width = width;
    info.type = deduce_physical_type_from_name(hierarchical_name, type_str);

    if (info.type == VcdSignalPhysicalType::PADDR) {
        m_paddr_width = width;
    } else if (info.type == VcdSignalPhysicalType::PWDATA) {
        m_pwdata_width = width;
    }

    m_signal_definitions[vcd_id_code] = info;
}

int SignalManager::get_paddr_width() const {
    return m_paddr_width;
}
int SignalManager::get_pwdata_width() const {
    return m_pwdata_width;
}

uint32_t SignalManager::parse_vcd_value_to_uint(const std::string& value_str, int bit_width, bool& out_has_x_or_z) {
    out_has_x_or_z = false;
    if (value_str.empty()) {
        out_has_x_or_z = true;
        return 0;
    }

    std::string num_part = value_str;
    char first_char_val_str = tolower(value_str[0]);

    if (value_str.length() == 1) {
        if (first_char_val_str == '0')
            return 0;
        if (first_char_val_str == '1')
            return 1;
        if (first_char_val_str == 'x' || first_char_val_str == 'z') {  // 雖然 QA 說無 'z'，但 VCD 標準包含它
            out_has_x_or_z = true;
            return 0;
        }

        out_has_x_or_z = true;
        return 0;
    }

    if (first_char_val_str == 'b') {
        if (value_str.length() > 1) {
            num_part = value_str.substr(1);
        } else {
            out_has_x_or_z = true;
            return 0;
        }
    }
    if (num_part.empty()) {
        out_has_x_or_z = true;
        return 0;
    }

    uint32_t result = 0;
    for (char c_orig : num_part) {
        char c = tolower(c_orig);
        result <<= 1;  // 左移一位準備下一位元
        if (c == '1') {
            result |= 1;
        } else if (c == '0') {
        } else if (c == 'x' || c == 'z') {
            out_has_x_or_z = true;
        } else {
            out_has_x_or_z = true;
            return 0;
        }
    }
    return result;
}

bool SignalManager::update_state_on_signal_change(
    const std::string& vcd_id_code,
    const std::string& value_str,
    SignalState& current_overall_state,
    bool& previous_pclk_val  // in-out parameter
) {
    bool pclk_rose_this_event = false;
    auto it = m_signal_definitions.find(vcd_id_code);
    if (it == m_signal_definitions.end()) {
        return false;
    }
    const VcdSignalInfo& sig_info = it->second;

    bool val_has_x = false;
    uint32_t new_uint_val = parse_vcd_value_to_uint(value_str, sig_info.bit_width, val_has_x);
    switch (sig_info.type) {
        case VcdSignalPhysicalType::PCLK: {
            bool new_pclk_state = (new_uint_val != 0);
            if (new_pclk_state && !previous_pclk_val) {
                pclk_rose_this_event = true;
            }
            current_overall_state.pclk = new_pclk_state;
            previous_pclk_val = new_pclk_state;
        } break;
        case VcdSignalPhysicalType::PRESETN:
            current_overall_state.presetn = (new_uint_val != 0);
            break;
        case VcdSignalPhysicalType::PADDR:
            current_overall_state.paddr = new_uint_val;
            current_overall_state.paddr_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PWRITE:
            current_overall_state.pwrite = (new_uint_val != 0);
            current_overall_state.pwrite_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PSEL:
            current_overall_state.psel = (new_uint_val != 0);
            current_overall_state.psel_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PENABLE:
            current_overall_state.penable = (new_uint_val != 0);
            current_overall_state.penable_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PWDATA:
            current_overall_state.pwdata = new_uint_val;
            current_overall_state.pwdata_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PRDATA:
            current_overall_state.prdata = new_uint_val;
            current_overall_state.prdata_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PREADY:
            current_overall_state.pready = (new_uint_val != 0);
            current_overall_state.pready_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PARAMETER:
        case VcdSignalPhysicalType::OTHER:
            break;
    }
    return pclk_rose_this_event;
}

const VcdSignalInfo* SignalManager::get_signal_info_by_vcd_id(const std::string& vcd_id_code) const {
    auto it = m_signal_definitions.find(vcd_id_code);
    if (it != m_signal_definitions.end()) {
        return &(it->second);
    }
    return nullptr;
}
}  // namespace APBSystem