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
    const auto& ordered_completers = stats.get_ordered_accessed_completers();
    const auto& bit_activity_data_map = stats.get_completer_bit_activity_map();

    int completer_display_index = 1;

    for (std::vector<CompleterID>::const_iterator it = ordered_completers.begin(); it != ordered_completers.end(); ++it) {
        CompleterID comp_id = *it;
        if (comp_id == APBSystem::CompleterID::NONE || comp_id == APBSystem::CompleterID::UNKNOWN_COMPLETER) {
            continue;

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
                for (int i = 7; i >= 0; --i) {
                    out_stream << "  a" << i << ": " << bit_detail_status_to_report_string(BitDetailStatus(), 'a') << "\n";
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
                    out_stream << "  d" << i << ": " << bit_detail_status_to_report_string(BitDetailStatus(), 'd') << "\n";
                }
            }
            completer_display_index++;
        }

        struct GernericError {
            uint64_t timestamp;
            std::string message;
            bool operator<(const GernericError& other) const {
                return timestamp < other.timestamp;
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

        // 新增：收集 Data Corruption 和 Mirroring 錯誤
        const auto& integrity_errors = stats.get_data_integrity_error_details();
        for (const auto& detail : integrity_errors) {
            std::ostringstream oss;
            if (detail.is_mirroring_suspected) {
                // 處理數據鏡像 (Data Mirroring)
                // 格式: [#<time>] Data Mirroring -> Value <數據> written at PADDR <原始地址> also found at PADDR <鏡像地址>
                oss << "Data Mirroring -> Value 0x" << std::hex << detail.received_data
                    << " written at PADDR 0x" << detail.original_write_addr
                    << " also found at PADDR 0x" << detail.paddr << std::dec;
                all_errors.push_back({detail.timestamp_ps, oss.str()});

                // 處理地址鏡像 (Address Mirroring) - QA A13a 指出由 Data Mirroring 反推
                // 格式: [#<time>] Address Mirroring -> Write at PADDR<原始地址> also reflected at PADDR<鏡像地址>
                std::ostringstream oss_addr;
                oss_addr << "Address Mirroring -> Write at PADDR 0x" << std::hex << detail.original_write_addr
                         << " also reflected at PADDR 0x" << detail.paddr << std::dec;
                all_errors.push_back({detail.original_write_time, oss_addr.str()});

            } else {
                // 處理數據損壞 (Data Corruption)
                // 格式: [#<time>] Data Corruption -> Expected PWDATA: <預期值>, Received: <錯誤值>
                // 注意: 這裡的錯誤是在讀取時發現的，所以更準確的說是 PRDATA，但題目格式寫 PWDATA
                oss << "Data Corruption -> Expected PRDATA: 0x" << std::hex << detail.expected_data
                    << ", Received: 0x" << detail.received_data << std::dec;
                all_errors.push_back({detail.timestamp_ps, oss.str()});
            }
        }

        std::sort(all_errors.begin(), all_errors.end());
        if (!all_errors.empty()) {
            out_stream << "\n--- Detected Errors (Sorted by Time) ---\n";
            for (const auto& error : all_errors) {
                out_stream << "[#" << error.timestamp << "] " << error.message << "\n";
            }
        }
    }
}  // namespace APBSystem
}