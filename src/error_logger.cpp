#include "error_logger.hpp"
#include <iomanip>
#include <sstream>

void ErrorLogger::logAddressMirroringError(uint64_t time, uint32_t original_address) {
    std::stringstream ss;
    ss << "[#" << time << "] Address Mirroring Error at 0x"
       << std::hex << std::setw(8) << std::setfill('0') << original_address;
    errors_.push_back(ss.str());
}

void ErrorLogger::logDataCorruptionError(uint64_t time, uint32_t address, uint32_t original_data) {
    std::stringstream ss;
    ss << "[#" << time << "] Data Corruption Error at 0x"
       << std::hex << std::setw(8) << std::setfill('0') << address
       << ", Original Data: 0x" << std::hex << std::setw(8) << std::setfill('0') << original_data;
    errors_.push_back(ss.str());
}

/**
 * @brief 【新增】實作交易超時錯誤的記錄邏輯。
 * 根據Q&A文件，格式化輸出超時錯誤訊息。
 */
void ErrorLogger::logTransactionTimeoutError(uint64_t time, uint32_t address) {
    std::stringstream ss;
    ss << "[#" << time << "] Timeout Occurred Transaction Stalled at PADDR 0x"
       << std::hex << std::setw(8) << std::setfill('0') << address;
    errors_.push_back(ss.str());
}

const std::vector<std::string>& ErrorLogger::getErrors() const {
    return errors_;
}
