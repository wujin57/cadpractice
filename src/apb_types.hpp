#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>
namespace APBSystem {
// --- 常數定義 ---
constexpr int MAX_COMPLETERS = 3;
constexpr int MAX_COMPLETERS_TO_TRACK = MAX_COMPLETERS;
constexpr int MAX_SIGNAL_BITS = 32;   // 假設PADDR/PWDATA最多32位元
constexpr int PADDR_REPORT_BITS = 8;  // 根據輸出範例，PADDR/PWDATA連接報告是a0-a7, d0-d7
constexpr int PWDATA_REPORT_BITS = 8;
constexpr int MIN_OBSERVATIONS_FOR_FAULT = 3;  // 來自您原始程式碼
constexpr int MAX_TIMEOUT_PCLK_CYCLES = 100;   // 來自您原始程式碼

// --- APB訊號狀態 ---
struct SignalState {
    uint32_t paddr = 0;
    bool paddr_has_x = false;
    uint32_t pwdata = 0;
    bool pwdata_has_x = false;
    uint32_t prdata = 0;
    bool prdata_has_x = false;
    bool pwrite = false;
    bool psel = false;  // 單一PSEL訊號的狀態
    bool penable = false;
    bool pready = false;
    bool presetn = true;  // 通常初始為高（非重設狀態）
    bool pclk = false;
};

// --- 故障分析相關 ---
struct BitPairComparisonStats {
    int equal_count = 0;
    int diff_count = 0;
};

struct IdentifiedFloatingPair {
    bool isActive = false;
    int bit1 = -1;  // 較小的位元索引
    int bit2 = -1;  // 较大的位元索引
    // 可以增加 PADDR 或 PWDATA 的標記
};

// --- APB FSM 狀態 ---
enum class ApbFsmState { IDLE,
                         SETUP,
                         ACCESS };

// --- 交易資料 (用於統計) ---
struct TransactionData {
    int start_time_vcd;  // VCD時間戳記
    int duration_pclk_cycles;
    uint32_t paddr;
    bool is_write;
    bool has_wait_states;
    int completer_id;
    // 可以加入其他需要的欄位，例如是否出錯
};
}