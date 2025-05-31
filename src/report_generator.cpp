// report_generator.cpp
#include "report_generator.hpp"
#include <iomanip>  // 用於 std::fixed, std::setprecision

namespace APBSystem {
ReportGenerator::ReportGenerator() {
}

void ReportGenerator::generate_apb_transaction_report(const Statistics& stats, std::ostream& out_stream) const {
    out_stream << "## APB Transaction Statistics Report ##\n";
    out_stream << "---------------------------------------\n";

    out_stream << "1. Number of Read Transactions with no wait states: "
               << stats.get_read_transactions_no_wait() << "\n";
    out_stream << "2. Number of Read Transactions with wait states:    "
               << stats.get_read_transactions_with_wait() << "\n";
    out_stream << "3. Number of Write Transactions with no wait states: "
               << stats.get_write_transactions_no_wait() << "\n";
    out_stream << "4. Number of Write Transactions with wait states:   "
               << stats.get_write_transactions_with_wait() << "\n";

    out_stream << std::fixed << std::setprecision(2);  // 設定浮點數輸出精度
    out_stream << "5. Average Read Cycle Duration (PCLK edges):        "
               << stats.get_average_read_cycle_duration() << "\n";
    out_stream << "6. Average Write Cycle Duration (PCLK edges):       "
               << stats.get_average_write_cycle_duration() << "\n";
    out_stream << "7. Bus Utilization (%):                             "
               << stats.get_bus_utilization_percentage() << " %\n";

    out_stream << std::defaultfloat;     // 先恢復預設，再設定整數輸出
    out_stream << std::setprecision(0);  // 對於整數計數，小數點後不需要位數

    out_stream << "8. Number of Idle PCLK edges:                       "
               << stats.get_num_idle_pclk_edges() << "\n";
    out_stream << "9. Number of Unique Completers Accessed:            "
               << stats.get_number_of_unique_completers_accessed() << "\n";

    out_stream << std::fixed << std::setprecision(2);  // 再次設定 CPU 時間的精度
    out_stream << "10. CPU Elapsed Time for Analysis (ms):             "
               << stats.get_cpu_elapsed_time_ms() << " ms\n";

    out_stream << std::defaultfloat << std::setprecision(6);  // 完全恢復預設浮點數格式

    out_stream << "---------------------------------------\n";
    out_stream << "Additional Info (for verification):\n";
    out_stream << "- Total PCLK rising edges detected: " << stats.get_total_pclk_edges() << "\n";
    out_stream << "- Total PCLK rising edges during Bus Active (PSEL high): " << stats.get_total_bus_active_pclk_edges() << "\n";
    out_stream << "---------------------------------------\n\n";
}

}  // namespace APBSystem
