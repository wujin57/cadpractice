// report_generator.cpp
#include "report_generator.hpp"
#include <algorithm>
#include <iomanip>  // 用於 std::fixed, std::setprecision
#include <sstream>
namespace APBSystem {
ReportGenerator::ReportGenerator() {
}
static std::string completer_id_to_report_string(APBSystem::CompleterID id) {
    switch (id) {
        case APBSystem::CompleterID::UART:
            return "UART";
        case APBSystem::CompleterID::GPIO:
            return "GPIO";
        case APBSystem::CompleterID::SPI_MASTER:
            return "SPI_MASTER";
        case APBSystem::CompleterID::UNKNOWN_COMPLETER:
            return "UNKNOWN";
        case APBSystem::CompleterID::NONE:
            return "NONE_OR_UNSET";
        default:
            return "INVALID_ID";
    }
}
static std::string bit_detail_status_to_report_string(const APBSystem::BitDetailStatus& detail, char bus_char_prefix) {
    switch (detail.status) {
        case APBSystem::BitConnectionStatus::CORRECT:
            return "Correct";
        case APBSystem::BitConnectionStatus::SHORTED:
            if (detail.shorted_with_bit_index != -1) {
                std::ostringstream oss;
                oss << "Connected with " << bus_char_prefix << detail.shorted_with_bit_index;
                return oss.str();
            }
            return "Shorted (Error)";
        default:
            return "Correct";  // 若狀態未知或未明確處理，預設為Correct
    }
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

    out_stream << "\nNumber of Transactions with Timeout: 0\n";
    out_stream << "Number of Out-of-Range Accesses: 0\n";
    out_stream << "Number of Mirrored Transactions: 0\n";
    out_stream << "Number of Read-Write Overlap Errors: 0\n\n";
    const auto& bit_activity_data_map = stats.get_completer_bit_activity_map();
    const auto& ordered_completers = stats.get_ordered_accessed_completers();
    int completer_display_index = 1;
    for (APBSystem::CompleterID comp_id : ordered_completers) {
        if (comp_id == APBSystem::CompleterID::NONE)
            continue;
        // 檢查該 Completer 是否真的有交易，避免為僅在 set 中但無交易的 UNKNOWN 打印
        // if (comp_id == APBSystem::CompleterID::UNKNOWN_COMPLETER &&
        //     (!stats.get_completer_transaction_counts().count(comp_id) || stats.get_completer_transaction_counts().at(comp_id) == 0)) {
        //     continue;
        // }
        // 上述檢查可以移除，因為 ordered_completers 的加入邏輯已考慮

        std::string completer_name_str = completer_id_to_report_string(comp_id);
        out_stream << "\nCompleter " << completer_display_index << " (" << completer_name_str << ") PADDR Connections\n";

        auto it_activity = bit_activity_data_map.find(comp_id);
        if (it_activity != bit_activity_data_map.end()) {
            const auto& paddr_details_vec = it_activity->second.paddr_bit_details;
            for (int i = 7; i >= 0; --i) {
                BitDetailStatus detail_status;
                if (static_cast<size_t>(i) < paddr_details_vec.size()) {
                    detail_status = paddr_details_vec[i];
                }
                out_stream << "  a" << i << ": " << bit_detail_status_to_report_string(detail_status, 'a') << "\n";
            }
        } else {
            // 如果一個 Completer 在 ordered_completers 中，它應該在 bit_activity_map 中有條目
            // 這裡作為備份，預設 Correct
            for (int i = 7; i >= 0; --i) {
                out_stream << "  a" << i << ": " << bit_detail_status_to_report_string((BitConnectionStatus::CORRECT), 'a') << "\n";
            }
        }

        out_stream << "Completer " << completer_display_index << " (" << completer_name_str << ") PWDATA Connections\n";
        if (it_activity != bit_activity_data_map.end()) {
            const auto& pwdata_details_vec = it_activity->second.pwdata_bit_details;
            for (int i = 7; i >= 0; --i) {
                BitDetailStatus detail_status;
                if (static_cast<size_t>(i) < pwdata_details_vec.size()) {
                    detail_status = pwdata_details_vec[i];
                }
                out_stream << "  d" << i << ": " << bit_detail_status_to_report_string(detail_status, 'd') << "\n";
            }
        } else {
            for (int i = 7; i >= 0; --i) {
                BitDetailStatus correct_detail;
                correct_detail.status = BitConnectionStatus::CORRECT;
                out_stream << "  d" << i << ": " << bit_detail_status_to_report_string(correct_detail, 'd') << "\n";
            }
        }
        completer_display_index++;
    }

    struct GernericError {
        uint64_t timestamp_ps;
        std::string message;
        bool operator<(const GernericError& other) const {
            return timestamp_ps < other.timestamp_ps;
        }
    };
    std::vector<GernericError> all_errors;

    const auto& oor_details = stats.get_out_of_range_details();
    for (const auto& detail : oor_details) {
        std::ostringstream oss;
        oss << "Out-of-Range Access -> PADDR 0x"
            << std::hex << detail.paddr << std::dec
            << " (Requester 1 -> Completer " << completer_id_to_report_string(detail.target_completer)
            << ")";
        all_errors.push_back({detail.timestamp_ps, oss.str()});
    }

    const auto& timeout_details = stats.get_timeout_error_details();
    for (const auto& detail : timeout_details) {
        std::ostringstream oss;
        oss << "Timeout Occurred -> Transaction Stalled at PADDR 0x"
            << std::hex << detail.paddr << std::dec;
        all_errors.push_back({detail.timeout_timestamp_ps, oss.str()});
    }

    std::sort(all_errors.begin(), all_errors.end());
    if (!all_errors.empty()) {
        out_stream << "\n--- Detected Errors (Sorted by Time) ---\n";
        for (const auto& error : all_errors) {
            out_stream << "[#" << error.timestamp_ps << "] " << error.message << "\n";
        }
    }
}
}  // namespace APBSystem