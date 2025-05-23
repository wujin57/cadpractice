// signal_manager.cpp
#include "signal_manager.hpp"
#include <algorithm>  // For std::max
#include <iostream>

namespace APBSystem {

SignalManager::SignalManager() {}

VcdSignalPhysicalType SignalManager::string_to_vcd_signal_type(const std::string& signal_name_suffix, const std::string& type_str) {
    if (type_str == "parameter")
        return VcdSignalPhysicalType::PARAMETER;

    if (signal_name_suffix == "clk")
        return VcdSignalPhysicalType::PCLK;
    if (signal_name_suffix == "rst_n")
        return VcdSignalPhysicalType::PRESETN;
    if (signal_name_suffix == "paddr")
        return VcdSignalPhysicalType::PADDR;
    if (signal_name_suffix == "pwrite")
        return VcdSignalPhysicalType::PWRITE;
    if (signal_name_suffix == "psel")
        return VcdSignalPhysicalType::PSEL;
    if (signal_name_suffix == "penable")
        return VcdSignalPhysicalType::PENABLE;
    if (signal_name_suffix == "pwdata")
        return VcdSignalPhysicalType::PWDATA;
    if (signal_name_suffix == "prdata")
        return VcdSignalPhysicalType::PRDATA;
    if (signal_name_suffix == "pready")
        return VcdSignalPhysicalType::PREADY;
    if (signal_name_suffix == "pslverr")
        return VcdSignalPhysicalType::PSLVERR;
    return VcdSignalPhysicalType::OTHER;
}

void SignalManager::register_signal(const std::string& vcd_id_code,
                                    const std::string& type_str,
                                    int width,
                                    const std::string& full_name) {
    VcdSignalInfo info;
    info.name = full_name;
    info.bit_width = width;

    size_t last_dot = full_name.rfind('.');
    std::string short_name = (last_dot == std::string::npos) ? full_name : full_name.substr(last_dot + 1);
    // 移除可能的陣列索引，例如 "paddr [31:0]" -> "paddr"
    size_t bracket_pos = short_name.find('[');
    if (bracket_pos != std::string::npos) {
        short_name = short_name.substr(0, bracket_pos);
        // 去除尾隨空格
        size_t last_char = short_name.find_last_not_of(" \t\n\r\f\v");
        if (std::string::npos != last_char) {
            short_name.erase(last_char + 1);
        }
    }

    info.type = string_to_vcd_signal_type(short_name, type_str);

    if (info.type == VcdSignalPhysicalType::PADDR) {
        paddr_width_ = std::max(paddr_width_, width);
    } else if (info.type == VcdSignalPhysicalType::PWDATA) {
        pwdata_width_ = std::max(pwdata_width_, width);
    }

    vcd_id_to_info_map_[vcd_id_code] = info;
}

uint32_t SignalManager::parse_vcd_value(const std::string& value_str, bool& has_x_or_z) const {
    uint32_t val = 0;
    has_x_or_z = false;
    if (value_str.empty())
        return 0;

    const char* s = value_str.c_str();
    if (value_str[0] == 'b' || value_str[0] == 'B') {
        s++;
        while (*s) {
            if (*s == '0')
                val = (val << 1);
            else if (*s == '1')
                val = (val << 1) | 1;
            else if (*s == 'x' || *s == 'X' || *s == 'z' || *s == 'Z') {
                val = (val << 1);  // 暫時將X/Z當作0處理，但標記其存在
                has_x_or_z = true;
            } else
                break;
            s++;
        }
    } else if (value_str.length() == 1) {
        if (value_str[0] == '1')
            val = 1;
        else if (value_str[0] == '0')
            val = 0;
        else if (value_str[0] == 'x' || value_str[0] == 'X' || value_str[0] == 'z' || value_str[0] == 'Z') {
            val = 0;
            has_x_or_z = true;
        }
    }
    return val;
}

bool SignalManager::update_signal_state_from_vcd(const std::string& vcd_id_code,
                                                 const std::string& value_str,
                                                 SignalState& target_state,
                                                 bool previous_pclk_state) {
    auto it = vcd_id_to_info_map_.find(vcd_id_code);
    if (it == vcd_id_to_info_map_.end()) {
        return false;  // VCD ID 未註冊
    }

    const VcdSignalInfo& info = it->second;
    if (info.type == VcdSignalPhysicalType::PARAMETER)
        return false;  // 參數值不更新狀態

    bool val_has_x = false;
    uint32_t new_uint_val = parse_vcd_value(value_str, val_has_x);
    bool pclk_rising_edge = false;

    switch (info.type) {
        case VcdSignalPhysicalType::PCLK:
            target_state.pclk = (new_uint_val != 0);
            if (target_state.pclk && !previous_pclk_state) {
                pclk_rising_edge = true;
            }
            break;
        case VcdSignalPhysicalType::PRESETN:
            target_state.presetn = (new_uint_val != 0);
            break;
        case VcdSignalPhysicalType::PADDR:
            target_state.paddr = new_uint_val;
            target_state.paddr_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PWRITE:
            target_state.pwrite = (new_uint_val != 0);
            break;
        case VcdSignalPhysicalType::PSEL:
            target_state.psel = (new_uint_val != 0);
            break;
        case VcdSignalPhysicalType::PENABLE:
            target_state.penable = (new_uint_val != 0);
            break;
        case VcdSignalPhysicalType::PWDATA:
            target_state.pwdata = new_uint_val;
            target_state.pwdata_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PRDATA:
            target_state.prdata = new_uint_val;
            target_state.prdata_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PREADY:
            target_state.pready = (new_uint_val != 0);
            break;
        case VcdSignalPhysicalType::PSLVERR:
            target_state.pslverr = (new_uint_val != 0);
            break;
        case VcdSignalPhysicalType::OTHER:
            break;
        case VcdSignalPhysicalType::PARAMETER:
            break;  // 已在開頭處理
    }
    return pclk_rising_edge;
}

}  // namespace APBSystem