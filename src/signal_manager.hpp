// signal_manager.hpp
#pragma once

#include <map>
#include <string>
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
        const std::string& vcd_id_code,
        const std::string& value_str,
        SignalState& current_overall_state,
        bool& previous_pclk_val);

    // 根據訊號的層次化名稱獲取其 VCD ID (如果 VCD Parser 主要使用 ID)
    // 或者 ApbAnalyzer 直接使用層次化名稱，則此函式可能不需要
    // const std::string* get_vcd_id_for_signal(const std::string& hierarchical_name) const;

    // (可選) 獲取特定 VCD ID 的訊號資訊，主要用於調試
    const VcdSignalInfo* get_signal_info_by_vcd_id(const std::string& vcd_id_code) const;

   private:
    // 使用 VCD ID code 作為 map 的 key，儲存已註冊訊號的詳細資訊
    std::map<std::string /*vcd_id_code*/, VcdSignalInfo> m_signal_definitions;

    // 輔助函式：根據訊號的層次化名稱的最後一部分來猜測其物理類型
    VcdSignalPhysicalType deduce_physical_type_from_name(const std::string& hierarchical_name, const std::string& vcd_type_str);

    // 輔助函式：將 VCD 中的值字串 (如 "b01x1", "hF", "1") 解析為 uint32_t，並檢測是否有 'x'
    // @param bit_width: 訊號的位元寬度，用於輔助解析或警告
    // @param has_x_or_z: (out) 解析出的值是否包含 'x' 或 'z'
    uint32_t parse_vcd_value_to_uint(const std::string& value_str, int bit_width, bool& out_has_x_or_z);
};

}  // namespace APBSystem
