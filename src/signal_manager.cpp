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

    if (name_suffix == "clk")
        return VcdSignalPhysicalType::PCLK;
    if (name_suffix == "rst_n")
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

uint32_t SignalManager::parse_vcd_value_to_uint(const char* value_ptr, size_t value_len, bool& out_has_x_or_z) {
    out_has_x_or_z = false;
    if (value_len == 0) {
        out_has_x_or_z = true;
        return 0;
    }

    size_t start_idx = 0;
    if (value_ptr[0] == 'b' || value_ptr[0] == 'B') {
        start_idx = 1;
    }

    if (start_idx >= value_len) {
        out_has_x_or_z = true;
        return 0;
    }

    uint32_t result = 0;
    for (size_t i = start_idx; i < value_len; ++i) {
        char c = value_ptr[i];
        if (c == '0' || c == '1') {
            result = (result << 1) | (c == '1');
        } else if (c == 'x' || c == 'X' || c == 'z' || c == 'Z') {
            result <<= 1;
            out_has_x_or_z = true;
        } else {
            continue;
        }
    }
    return result;
}

bool SignalManager::update_state_on_signal_change(
    char vcd_id_char,
    const char* value_ptr,
    size_t value_len,
    SignalState& current_overall_state,
    bool& previous_pclk_val) {
    std::string id_str(1, vcd_id_char);
    auto it = m_signal_definitions.find(id_str);
    if (it == m_signal_definitions.end()) {
        return false;
    }
    const VcdSignalInfo& sig_info = it->second;

    bool val_has_x = false;
    uint32_t new_uint_val = parse_vcd_value_to_uint(value_ptr, value_len, val_has_x);

    bool pclk_rose_this_event = false;
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
        default:
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