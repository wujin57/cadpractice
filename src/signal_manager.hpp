// signal_manager.hpp
#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "apb_types.hpp"  // 包含 SignalState, VcdSignalPhysicalType, VcdSignalInfo

namespace APBSystem {

class SignalManager {
   public:
    SignalManager();

    void register_signal(const std::string& vcd_id_code,
                         const std::string& type_str,
                         int width,
                         const std::string& hierarchical_name);

    bool update_state_on_signal_change(
        char vcd_id_char,
        const char* value_ptr,
        size_t value_len,
        SignalState& current_overall_state,
        bool& previous_pclk_val);

    const VcdSignalInfo* get_signal_info_by_vcd_id(const std::string& vcd_id_code) const;
    int get_paddr_width() const;
    int get_pwdata_width() const;

   private:
    // 使用 VCD ID code 作為 map 的 key，儲存已註冊訊號的詳細資訊
    std::unordered_map<std::string, VcdSignalInfo> m_signal_definitions;

    int m_paddr_width{32};
    int m_pwdata_width{32};
    VcdSignalPhysicalType deduce_physical_type_from_name(const std::string& hierarchical_name, const std::string& vcd_type_str);

    uint32_t parse_vcd_value_to_uint(const char* value_ptr, size_t value_len, bool& out_has_x_or_z);
};

}  // namespace APBSystem
