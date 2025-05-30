#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace APBSystem {
// --- 常數定義 ---
constexpr int MAX_COMPLETERS = 3;
constexpr int MAX_TIMEOUT_PCLK_CYCLES = 100;

enum class ApbFsmState {
    IDLE,
    SETUP,
    ACCESS
};

// Structure to hold current values of relevant APB signals
struct SignalValues {
    bool PCLK_posedge = false;  // True if this is a posedge of PCLK
    bool PRESETn = true;        // Default to de-asserted
    uint32_t PADDR = 0;
    bool PSEL_UART = false;
    bool PSEL_GPIO = false;
    bool PSEL_SPI_MASTER = false;
    // Helper to check if any PSEL is active
    bool is_any_psel_active() const {
        return PSEL_UART || PSEL_GPIO || PSEL_SPI_MASTER;
    }
    bool PENABLE = false;
    bool PWRITE = false;
    uint32_t PWDATA = 0;
    uint32_t PRDATA = 0;
    bool PREADY = false;
};
enum class CompleterLogicalID : int { UART = 0,
                                      GPIO = 1,
                                      SPI_MASTER = 2,
                                      UNKNOWN = -1 };

// Structure to hold data for a completed transaction (focused on statistics)
struct TransactionData {
    long long transaction_id_ = 0;
    long long start_time_ = 0;  // VCD timestamp or PCLK cycle count
    long long end_time_ = 0;    // VCD timestamp or PCLK cycle count

    uint32_t paddr_ = 0;
    bool is_write_ = false;
    std::optional<uint32_t> pwdata_ = std::nullopt;
    std::optional<uint32_t> prdata_ = std::nullopt;

    enum class Status { OK,
                        ERROR } status_ = Status::OK;  // Basic status

    CompleterLogicalID completer_id_ = CompleterLogicalID::UNKNOWN;

    int duration_pclk_cycles_ = 0;  // Total PCLK cycles for the transaction (Setup + Access)
    bool has_wait_states_ = false;  // True if access phase > 1 PCLK cycle
};

// Structure for basic error reporting (can be kept simple)
struct ErrorDescriptor {
    long long timestamp = 0;
    std::string error_type_key;  // e.g., "E01", "E02" (from problem spec)
    std::string message;
    // Optionally, add paddr or other context
};

inline CompleterLogicalID get_completer_type_from_address(unsigned int paddr) {
    if (paddr >= 0x1A100000 && paddr <= 0x1A100FFF) {  // UART
        return CompleterLogicalID::UART;
    }
    if (paddr >= 0x1A101000 && paddr <= 0x1A101FFF) {  // GPIO
        return CompleterLogicalID::GPIO;
    }
    if (paddr >= 0x1A102000 && paddr <= 0x1A102FFF) {  // SPI_MASTER
        return CompleterLogicalID::SPI_MASTER;
    }
    return CompleterLogicalID::UNKNOWN;
}

inline std::string completer_id_to_string(CompleterLogicalID id) {
    switch (id) {
        case CompleterLogicalID::UART:
            return "UART";
        case CompleterLogicalID::GPIO:
            return "GPIO";
        case CompleterLogicalID::SPI_MASTER:
            return "SPI_MASTER";
        case CompleterLogicalID::UNKNOWN:
            return "UNKNOWN";
        default:
            return "INVALID_COMPLETER_ID";
    }
}

}  // namespace APBSystem