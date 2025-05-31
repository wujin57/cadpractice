#include "signal_manager.hpp"
#include <algorithm>  // For std::tolower, std::all_of
#include <iostream>   // For std::cerr (debugging)
#include <stdexcept>  // For std::invalid_argument, std::out_of_range

namespace APBSystem {

SignalManager::SignalManager() {}

// 根據訊號名稱後綴和 VCD 類型字串推斷物理類型
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

    // 去除可能的位元範圍標記，例如 "[31:0]" (雖然您說 VcdParser 可能已處理)
    // 為保險起見，這裡再處理一次，或者依賴 VcdParser 傳遞純淨的 name_suffix
    size_t bracket_pos = name_suffix.find('[');
    if (bracket_pos != std::string::npos) {
        name_suffix = name_suffix.substr(0, bracket_pos);
    }
    // 去除名稱前後的空格
    name_suffix.erase(0, name_suffix.find_first_not_of(" \t\n\r\f\v"));
    name_suffix.erase(name_suffix.find_last_not_of(" \t\n\r\f\v") + 1);

    if (name_suffix == "clk" || name_suffix == "pclk")
        return VcdSignalPhysicalType::PCLK;  // 兼容 pclk
    if (name_suffix == "rst_n" || name_suffix == "presetn")
        return VcdSignalPhysicalType::PRESETN;  // 兼容 presetn
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
    if (name_suffix == "pslverr")
        return VcdSignalPhysicalType::PSLVERR;

    // std::cerr << "Warning: Unknown signal type for name suffix: " << name_suffix << " (full: " << hierarchical_name << ")" << std::endl;
    return VcdSignalPhysicalType::OTHER;
}

void SignalManager::register_signal(const std::string& vcd_id_code,
                                    const std::string& type_str,
                                    int width,
                                    const std::string& hierarchical_name) {
    if (vcd_id_code.empty()) {
        // std::cerr << "Warning: Attempted to register signal with empty VCD ID code. Name: " << hierarchical_name << std::endl;
        return;
    }
    VcdSignalInfo info;
    info.hierarchical_name = hierarchical_name;
    info.bit_width = width;
    info.type = deduce_physical_type_from_name(hierarchical_name, type_str);

    m_signal_definitions[vcd_id_code] = info;
}

// 根據 QA 文件，訊號值（非時間）均為二進位格式。
// 時間戳為十進位 (由 VcdParser 的 time_cb 直接處理 std::stoull)。
uint32_t SignalManager::parse_vcd_value_to_uint(const std::string& value_str, int bit_width, bool& out_has_x_or_z) {
    out_has_x_or_z = false;
    if (value_str.empty()) {
        out_has_x_or_z = true;
        return 0;
    }

    std::string num_part = value_str;
    char first_char_val_str = tolower(value_str[0]);

    // 1. 處理單一位元值 "0", "1", "x", "z"
    if (value_str.length() == 1) {
        if (first_char_val_str == '0')
            return 0;
        if (first_char_val_str == '1')
            return 1;
        if (first_char_val_str == 'x' || first_char_val_str == 'z') {  // 雖然 QA 說無 'z'，但 VCD 標準包含它
            out_has_x_or_z = true;
            return 0;  // 'x' 或 'z' 的數值部分視為0
        }
        // 其他未知單一字元
        // std::cerr << "Warning: Single character VCD value '" << value_str << "' is not 0, 1, x, or z. Treated as x." << std::endl;
        out_has_x_or_z = true;
        return 0;
    }

    // 2. 處理多位元二進位值
    // 根據 QA，格式統一為二進位。VCD 標準中，二進位向量可以有 'b' 前綴，也可以省略。
    if (first_char_val_str == 'b') {
        if (value_str.length() > 1) {
            num_part = value_str.substr(1);
        } else {  // 只有 "b"，無效
            // std::cerr << "Warning: Invalid binary VCD value (only 'b'): " << value_str << std::endl;
            out_has_x_or_z = true;
            return 0;
        }
    } else {
        // 沒有 'b' 前綴，但 QA 說格式統一為二進位，所以假設它就是二進位字串 (例如 "0101x")
        // 或者它是單一位元 (已在上面處理)
        // 此處 num_part 保持為 value_str
    }

    if (num_part.empty()) {
        // std::cerr << "Warning: Empty numeric part after stripping prefix for VCD value: " << value_str << std::endl;
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
            // 保持 0，無需操作
        } else if (c == 'x' || c == 'z') {
            out_has_x_or_z = true;
            // 'x' 或 'z' 位元貢獻 0 到數值結果，但標記 out_has_x_or_z
        } else {
            // std::cerr << "Warning: Invalid character '" << c_orig << "' in binary VCD value: " << value_str << " (numeric part: " << num_part << ")" << std::endl;
            out_has_x_or_z = true;  // 包含非法字元，整個值視為 X
            return 0;               // 出錯時，安全返回0並標記X
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
        // Questo ID VCD non è stato registrato, potrebbe essere un commento o una direttiva non gestita.
        // Oppure un segnale che non ci interessa tracciare in SignalState.
        return false;
    }
    const VcdSignalInfo& sig_info = it->second;

    bool val_has_x = false;  // Renamed from val_has_x_or_z for clarity as problem statement says no 'z'
    uint32_t new_uint_val = parse_vcd_value_to_uint(value_str, sig_info.bit_width, val_has_x);

    // 更新 SignalState 中的對應成員
    switch (sig_info.type) {
        case VcdSignalPhysicalType::PCLK:
            // previous_pclk_val 是輸入的上一個週期的 PCLK 狀態
            // current_overall_state.pclk 是此事件發生 *之前* 的 PCLK 狀態 (如果 PCLK 不是此事件的主角)
            // 或者如果 PCLK 是此事件的主角，它將被 new_uint_val 更新
            {
                bool new_pclk_state = (new_uint_val != 0);
                if (new_pclk_state && !previous_pclk_val) {  // 從 0 變到 1
                    pclk_rose_this_event = true;
                }
                current_overall_state.pclk = new_pclk_state;  // 更新 SignalState 中的 PCLK
                previous_pclk_val = new_pclk_state;           // 更新外部追蹤的 PCLK 前一個值
            }
            break;
        case VcdSignalPhysicalType::PRESETN:
            current_overall_state.presetn = (new_uint_val != 0);
            // 一般來說，presetn 的 'x' 狀態也需要關注，但題目可能簡化
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
        case VcdSignalPhysicalType::PSLVERR:
            current_overall_state.pslverr = (new_uint_val != 0);
            current_overall_state.pslverr_has_x = val_has_x;
            break;
        case VcdSignalPhysicalType::PARAMETER:
        case VcdSignalPhysicalType::OTHER:
            // Parameters and other unhandled types are ignored for SignalState update
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