#ifndef ERROR_LOGGER_HPP
#define ERROR_LOGGER_HPP

#include <cstdint>
#include <string>
#include <vector>

/**
 * @class ErrorLogger
 * @brief 負責記錄和管理在APB交易分析過程中偵測到的所有錯誤。
 */
class ErrorLogger {
   public:
    /**
     * @brief 記錄位址鏡像錯誤 (Address Mirroring Error)。
     * @param time 錯誤發生的時間戳。
     * @param original_address 交易開始時的原始位址。
     */
    void logAddressMirroringError(uint64_t time, uint32_t original_address);

    /**
     * @brief 記錄資料毀壞錯誤 (Data Corruption Error)。
     * @param time 錯誤發生的時間戳。
     * @param address 發生錯誤的交易位址。
     * @param original_data 交易開始時的原始寫入資料。
     */
    void logDataCorruptionError(uint64_t time, uint32_t address, uint32_t original_data);

    /**
     * @brief 【新增】記錄交易超時錯誤 (Transaction Timeout Error)。
     * @param time 交易開始的時間戳。
     * @param address 發生超時的交易位址。
     */
    void logTransactionTimeoutError(uint64_t time, uint32_t address);

    /**
     * @brief 取得所有已記錄的錯誤訊息。
     * @return 一個包含所有錯誤字串的 vector。
     */
    const std::vector<std::string>& getErrors() const;

   private:
    std::vector<std::string> errors_;  // 儲存所有錯誤訊息的容器
};

#endif  // ERROR_LOGGER_HPP
