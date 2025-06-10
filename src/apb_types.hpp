#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>
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
    UNKNOWN_COMPLETER,
    NONE
};

const uint32_t UART_BASE_ADDR = 0x1A100000;
const uint32_t UART_END_ADDR = 0x1A100FFF;
const uint32_t GPIO_BASE_ADDR = 0x1A101000;
const uint32_t GPIO_END_ADDR = 0x1A101FFF;
const uint32_t SPI_MASTER_BASE_ADDR = 0x1A102000;
const uint32_t SPI_MASTER_END_ADDR = 0x1A102FFF;

struct TransactionInfo {
    bool active = false;
    uint64_t start_pclk_edge_count = 0;
    uint64_t transaction_start_time_ps = 0;  // 交易開始的 VCD 時間戳 (PSEL拉高時)
    bool is_write = false;
    uint32_t paddr = 0;
    bool paddr_val_has_x = false;
    uint32_t pwdata_val = 0;
    bool pwdata_val_has_x = false;
    bool had_wait_state = false;
    CompleterID target_completer = CompleterID::NONE;
    bool is_out_of_range = false;

    void reset() {
        active = false;
        start_pclk_edge_count = 0;
        transaction_start_time_ps = 0;
        is_write = false;
        paddr = 0;
        paddr_val_has_x = false;
        pwdata_val = 0;
        pwdata_val_has_x = false;
        had_wait_state = false;
        target_completer = CompleterID::NONE;
        is_out_of_range = false;
    }
};

struct SignalState {
    uint64_t timestamp_ps = 0;
    bool pclk = false;
    bool presetn = true;
    uint32_t paddr = 0;
    bool paddr_has_x = false;
    bool pwrite = false;
    bool pwrite_has_x = false;
    bool psel = false;
    bool psel_has_x = false;
    bool penable = false;
    bool penable_has_x = false;
    uint32_t pwdata = 0;
    bool pwdata_has_x = false;
    uint32_t prdata = 0;
    bool prdata_has_x = false;
    bool pready = false;
    bool pready_has_x = false;

    SignalState() {
        pclk = false;
        presetn = false;
        paddr = 0;
        paddr_has_x = true;
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
    }
};

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
    PARAMETER,
    OTHER
};
enum class BitConnectionStatus {
    CORRECT,
    SHORTED
};
struct VcdSignalInfo {
    std::string hierarchical_name;
    VcdSignalPhysicalType type = VcdSignalPhysicalType::OTHER;
    int bit_width = 1;
};
struct BitDetailStatus {
    BitConnectionStatus status;
    int shorted_with_bit_index;

    BitDetailStatus() : status(BitConnectionStatus::CORRECT), shorted_with_bit_index(-1) {}
    BitDetailStatus(BitConnectionStatus s)
        : status(s), shorted_with_bit_index(-1) {}
};

struct CompleterBitActivity {
    std::vector<BitDetailStatus> paddr_bit_details;   // 固定8位
    std::vector<BitDetailStatus> pwdata_bit_details;  // 固定8位

    CompleterBitActivity()
        : paddr_bit_details(8),  // 預設建構 BitDetailStatus (status=CORRECT, shorted_with_bit=-1)
          pwdata_bit_details(8) {}
};
struct OutOfRangeAccessDetail {
    uint64_t timestamp_ps;
    uint32_t paddr;
    CompleterID target_completer;
    bool is_write_transaction;
    bool prdata_had_x_on_oor_read;  // 仍然記錄 OOR 時 PRDATA 是否有 'x'
};

// 用於儲存原始 PADDR 和 PWDATA 值的樣本，以供短路分析
struct CompleterRawDataSamples {
    std::vector<uint32_t> paddr_samples;   // 只儲存不含 'x' 的 PADDR 值
    std::vector<uint32_t> pwdata_samples;  // 只儲存不含 'x' 的 PWDATA 值 (僅寫交易)
};

struct TransactionTimeoutDetail {
    uint64_t start_timestamp_ps;
    uint64_t timeout_timestamp_ps;
    uint32_t paddr;
    uint64_t exceeded_cycles;
};

struct DataIntegrityErrorDetail {
    uint64_t timestamp_ps;
    uint32_t paddr;
    uint32_t expected_data;
    uint32_t received_data;
    CompleterID target_completer;

    bool is_mirroring_suspected;
    uint32_t original_write_addr;
    uint64_t original_write_time;
};

struct ReverseWriteInfo {
    uint32_t address;
    uint64_t timestamp;
};
}  // namespace APBSystem
