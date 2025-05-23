// signal_manager.hpp
#pragma once

#include <map>
#include <string>
#include <vector>
#include "apb_types.hpp"  // 包含 SignalState 等

namespace APBSystem {

enum class VcdSignalPhysicalType {  // 與 apb_types.hpp 中的 SignalType 區分開，這是VCD訊號的物理類型
    PCLK,
    PRESETN,
    PADDR,
    PWRITE,
    PSEL,
    PENABLE,
    PWDATA,
    PRDATA,
    PREADY,
    PSLVERR,
    PARAMETER,
    OTHER
};

struct VcdSignalInfo {
    std::string name;  // VCD中的完整名稱
    VcdSignalPhysicalType type = VcdSignalPhysicalType::OTHER;
    int bit_width = 1;
    // 注意：由於VCD中只有一個psel，這裡的completer_id可能不直接從PSEL名稱解析
};

class SignalManager {
   public:
    SignalManager();

    void register_signal(const std::string& vcd_id_code,
                         const std::string& type_str,  // VCD中的 "wire", "reg", "parameter"
                         int width,
                         const std::string& full_name);

    // 更新提供的 SignalState
    // 返回 PCLK 是否有上升緣
    bool update_signal_state_from_vcd(const std::string& vcd_id_code,
                                      const std::string& value_str,
                                      SignalState& target_state,  // 要修改的狀態
                                      bool previous_pclk_state);  // 用於偵測PCLK邊緣

    int get_paddr_width() const { return paddr_width_ > 0 ? paddr_width_ : MAX_SIGNAL_BITS; }
    int get_pwdata_width() const { return pwdata_width_ > 0 ? pwdata_width_ : MAX_SIGNAL_BITS; }

   private:
    std::map<std::string, VcdSignalInfo> vcd_id_to_info_map_;
    VcdSignalPhysicalType string_to_vcd_signal_type(const std::string& signal_name_suffix, const std::string& type_str);
    uint32_t parse_vcd_value(const std::string& value_str, bool& has_x_or_z) const;

    int paddr_width_ = 0;
    int pwdata_width_ = 0;
};

}  // namespace APBSystem