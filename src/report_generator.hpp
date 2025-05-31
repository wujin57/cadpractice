#include <iostream>
#include <string>
#include "statistics.hpp"  // 依賴 Statistics 類別來獲取數據

namespace APBSystem {

class ReportGenerator {
   public:
    ReportGenerator();

    // 生成 APB 交易統計報表
    // @param stats: 包含所有統計數據的 Statistics 物件
    // @param out_stream: 報表輸出的目標流 (例如 std::cout 或一個檔案流)
    void generate_apb_transaction_report(const Statistics& stats, std::ostream& out_stream) const;

    // 您可能還有其他報表生成方法，例如錯誤摘要報表
    // void generate_error_summary_report(const ErrorLogger& error_logger, std::ostream& out_stream) const;
};

}  // namespace APBSystem