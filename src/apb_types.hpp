#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace APBSystem {

enum class ApbFsmState {
    IDLE,
    SETUP,
    ACCESS
};

enum class CompleterID {
    UART,
    GPIO,
    SPI_MASTER,
    UNKNOWN_COMPLETER
};

const uint32_t UART_BASE_ADDR = 0x1A100000;
const uint32_t UART_END_ADDR = 0x1A100FFF;
const uint32_t GPIO_BASE_ADDR = 0x1A101000;
const uint32_t GPIO_END_ADDR = 0x1A101FFF;
const uint32_t SPI_MASTER_BASE_ADDR = 0x1A102000;
const uint32_t SPI_MASTER_END_ADDR = 0x1A102FFF;

struct TransactionInfo {
    bool active = false;
    uint64_t start_pclk_edge_count = 0;      // 交易開始的 PCLK 上升沿計數
    uint64_t transaction_start_time_ps = 0;  // 交易開始的 VCD 時間戳 (PSEL拉高時)
    bool is_write = false;
    uint32_t paddr = 0;
    bool had_wait_state = false;

    void reset() {
        active = false;
        start_pclk_edge_count = 0;
        transaction_start_time_ps = 0;
        is_write = false;
        paddr = 0;
        had_wait_state = false;
    }
};

struct SignalState {
    uint64_t timestamp_ps = 0;  // 當前狀態對應的 VCD 時間戳

    // APB Interface Signals
    bool pclk = false;
    bool presetn = true;
    uint32_t paddr = 0;
    bool paddr_has_x = false;  // 標記 paddr 是否包含 'x'
    bool pwrite = false;
    bool pwrite_has_x = false;
    bool psel = false;
    bool psel_has_x = false;  // 標記 psel 是否為 'x'
    bool penable = false;
    bool penable_has_x = false;  // 標記 penable 是否為 'x'
    uint32_t pwdata = 0;
    bool pwdata_has_x = false;   // 標記 pwdata 是否包含 'x'
    uint32_t prdata = 0;         // 由 Completer 驅動
    bool prdata_has_x = false;   // 標記 prdata 是否包含 'x'
    bool pready = false;         // 由 Completer 驅動
    bool pready_has_x = false;   // 標記 pready 是否為 'x'
    bool pslverr = false;        // 由 Completer 驅動
    bool pslverr_has_x = false;  // 標記 pslverr 是否為 'x'

    SignalState() {  // 初始化構造函式
        pclk = false;
        presetn = false;  // 假設初始在復位狀態，VCD dumpvars 顯示 rst_n = 0
        paddr = 0;
        paddr_has_x = true;  // 初始地址未知是合理的
        pwrite = false;
        pwrite_has_x = true;
        psel = false;
        psel_has_x = true;
        penable = false;
        penable_has_x = true;
        pwdata = 0;
        pwdata_has_x = true;
        prdata = 0;
        prdata_has_x = true;
        pready = false;
        pready_has_x = true;
        pslverr = false;
        pslverr_has_x = true;
    }
};

// VCD 中訊號的物理類型 (用於 SignalManager 內部判斷)
enum class VcdSignalPhysicalType {
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
    OTHER  // 其他 VCD 變數類型
};

// 儲存 VCD 中訊號定義的資訊 (用於 SignalManager)
struct VcdSignalInfo {
    std::string hierarchical_name;
    VcdSignalPhysicalType type = VcdSignalPhysicalType::OTHER;
    int bit_width = 1;
    // std::string vcd_id_code; // VCD ID Code (例如 '#', '%') -> 這個將作為 map 的 key
};

}  // namespace APBSystem
