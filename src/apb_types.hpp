#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace APBSystem {

// --- 列舉與常數定義 (無變動) ---
enum class ApbFsmState { IDLE,
                         SETUP,
                         ACCESS };
enum class CompleterID { UART,
                         GPIO,
                         SPI_MASTER,
                         UNKNOWN_COMPLETER,
                         NONE };
const uint32_t UART_BASE_ADDR = 0x1A100000;
const uint32_t UART_END_ADDR = 0x1A100FFF;
const uint32_t GPIO_BASE_ADDR = 0x1A101000;
const uint32_t GPIO_END_ADDR = 0x1A101FFF;
const uint32_t SPI_MASTER_BASE_ADDR = 0x1A102000;
const uint32_t SPI_MASTER_END_ADDR = 0x1A102FFF;

// --- 交易與訊號狀態結構 (無變動) ---
struct TransactionInfo {
    bool active = false;
    uint64_t start_pclk_edge_count = 0;
    uint64_t transaction_start_time_ps = 0;
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
    SignalState() { /* 預設值與之前相同 */ }
};
enum class VcdSignalPhysicalType { PCLK,
                                   PRESETN,
                                   PADDR,
                                   PWRITE,
                                   PSEL,
                                   PENABLE,
                                   PWDATA,
                                   PRDATA,
                                   PREADY,
                                   PARAMETER,
                                   OTHER };
struct VcdSignalInfo {
    std::string hierarchical_name;
    VcdSignalPhysicalType type = VcdSignalPhysicalType::OTHER;
    int bit_width = 1;
};

// --- 位元狀態與錯誤結構 (MODIFIED) ---
enum class BitConnectionStatus { CORRECT,
                                 SHORTED };

// MODIFIED: 移除不再需要的旗標
struct BitDetailStatus {
    BitConnectionStatus status = BitConnectionStatus::CORRECT;
    int shorted_with_bit_index = -1;
};

struct CompleterBitActivity {
    std::vector<std::vector<std::array<int, 4>>> paddr_combinations;
    std::vector<std::vector<std::array<int, 4>>> pwdata_combinations;
    std::vector<BitDetailStatus> paddr_bit_details;
    std::vector<BitDetailStatus> pwdata_bit_details;
    void resize(int paddr_width, int pwdata_width) {
        if (paddr_bit_details.size() != paddr_width) {
            paddr_combinations.assign(paddr_width, std::vector<std::array<int, 4>>(paddr_width, {0, 0, 0, 0}));
            paddr_bit_details.assign(paddr_width, BitDetailStatus());
        }
        if (pwdata_bit_details.size() != pwdata_width) {
            pwdata_combinations.assign(pwdata_width, std::vector<std::array<int, 4>>(pwdata_width, {0, 0, 0, 0}));
            pwdata_bit_details.assign(pwdata_width, BitDetailStatus());
        }
    }
};

// --- 錯誤細節結構 (與前版相同) ---
struct OutOfRangeAccessDetail {
    uint64_t timestamp_ps;
    uint32_t paddr;
};
struct DataMirroringDetail {
    uint64_t read_timestamp;
    uint32_t mirrored_addr;
    uint32_t data_value;
    uint32_t original_write_addr;
    uint64_t original_write_time;
};
struct ReverseWriteInfo {
    uint32_t address;
    uint64_t timestamp;
};
struct TransactionTimeoutDetail {
    uint64_t start_timestamp_ps;
    uint32_t paddr;
};
struct ReadWriteOverlapDetail {
    uint64_t timestamp_ps;
    uint32_t paddr;
};
struct AddressCorruptionDetail {
    uint64_t timestamp;
    int bit_a, bit_b;
};
struct DataCorruptionDetail {
    uint64_t timestamp;
    int bit_a, bit_b;
};

}  // namespace APBSystem
