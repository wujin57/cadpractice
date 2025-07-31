// report_generator.cpp
#include "report_generator.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

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
        default:
            return "UNKNOWN";
    }
}
static std::string bit_detail_status_to_report_string(const APBSystem::BitDetailStatus& detail, char prefix) {
    if (detail.status == APBSystem::BitConnectionStatus::SHORTED) {
        std::ostringstream oss;
        oss << "Connected with " << prefix << detail.shorted_with_bit_index;
        return oss.str();
    }
    return "Correct";
}
void ReportGenerator::generate_apb_transaction_report(const Statistics& stats, std::ostream& out) const {
    // Section 1: Transaction Statistics
    out << "Number of Read Transactions with no wait states: " << stats.get_read_transactions_no_wait() << "\n";
    out << "Number of Read Transactions with wait states: " << stats.get_read_transactions_with_wait() << "\n";
    out << "Number of Write Transactions with no wait states: " << stats.get_write_transactions_no_wait() << "\n";
    out << "Number of Write Transactions with wait states: " << stats.get_write_transactions_with_wait() << "\n";
    out << std::fixed << std::setprecision(2);
    out << "Average Read Cycle: " << stats.get_average_read_cycle_duration() << " cycles\n";
    out << "Average Write Cycle: " << stats.get_average_write_cycle_duration() << " cycles\n";
    out << "Bus Utilization: " << stats.get_bus_utilization_percentage() << "%\n";
    out << std::defaultfloat << std::setprecision(0);
    out << "Number of Idle Cycles: " << stats.get_num_idle_pclk_edges() << "\n";
    out << "Number of Completer: " << stats.get_number_of_unique_completers_accessed() << "\n";
    out << std::fixed << std::setprecision(2);
    out << "CPU Elapsed Time: " << stats.get_cpu_elapsed_time_ms() << " ms\n";
    out << std::defaultfloat << std::setprecision(6);

    // Section 2: Error Summary
    out << "\nNumber of Transactions with Timeout: " << stats.get_timeout_error_details().size() << "\n";
    out << "Number of Out-of-Range Accesses: " << stats.get_out_of_range_details().size() << "\n";
    out << "Number of Mirrored Transactions: " << stats.get_mirroring_error_count() << "\n";
    out << "Number of Read-Write Overlap Errors: " << stats.get_read_write_overlap_details().size();

    // Section 3: Completer Connection Status
    const auto& activity_map = stats.get_completer_bit_activity_map();
    const std::vector<std::pair<int, CompleterID>> fixed_completer_order = {
        {1, CompleterID::UART},
        {2, CompleterID::GPIO},
        {3, CompleterID::SPI_MASTER}};
    for (const auto& comp_pair : fixed_completer_order) {
        int completer_num = comp_pair.first;
        CompleterID cid = comp_pair.second;

        auto it = activity_map.find(cid);
        if (it != activity_map.end()) {
            out << "\n\nCompleter " << completer_num << " PADDR Connections";
            const auto& paddr_details = it->second.paddr_bit_details;
            if (!paddr_details.empty()) {
                for (int j = paddr_details.size() - 1; j >= 0; --j) {
                    out << "\na" << std::setw(2) << std::setfill('0') << j << ": " << bit_detail_status_to_report_string(paddr_details[j], 'a');
                }
            }

            out << "\n\nCompleter " << completer_num << " PWDATA Connections";
            const auto& pwdata_details = it->second.pwdata_bit_details;
            if (!pwdata_details.empty()) {
                for (int j = pwdata_details.size() - 1; j >= 0; --j) {
                    out << "\nd" << std::setw(2) << std::setfill('0') << j << ": " << bit_detail_status_to_report_string(pwdata_details[j], 'd');
                }
            }
        }
    }

    // Section 4: Detailed Error Log
    struct ErrorLogEntry {
        uint64_t timestamp;
        std::string message;
        bool operator<(const ErrorLogEntry& other) const { return timestamp < other.timestamp; }
    };
    std::vector<ErrorLogEntry> errors;

    for (const auto& d : stats.get_out_of_range_details()) {
        errors.push_back({d.timestamp, "Out-of-Range Access -> PADDR 0x" + (std::stringstream() << std::hex << d.paddr).str()});
    }
    for (const auto& d : stats.get_timeout_error_details()) {
        errors.push_back({d.start_timestamp, "Timeout Occurred -> Transaction Stalled at PADDR 0x" + (std::stringstream() << std::hex << d.paddr).str()});
    }
    for (const auto& d : stats.get_read_write_overlap_details()) {
        errors.push_back({d.timestamp, "Read-Write Overlap Error -> Read & Write at PADDR 0x" + (std::stringstream() << std::hex << d.paddr).str() + " overlapped"});
    }
    for (const auto& d : stats.get_data_mirroring_details()) {
        errors.push_back({d.original_write_time, "Address Mirroring -> Write at PADDR 0x" + (std::stringstream() << std::hex << d.original_write_addr).str() + " also reflected at PADDR 0x" + (std::stringstream() << std::hex << d.mirrored_addr).str()});
        errors.push_back({d.read_timestamp, "Data Mirroring -> Value 0x" + (std::stringstream() << std::hex << d.data_value).str() + " written at PADDR 0x" + (std::stringstream() << std::hex << d.original_write_addr).str() + " also found at PADDR 0x" + (std::stringstream() << std::hex << d.mirrored_addr).str()});
    }
    std::sort(errors.begin(), errors.end());
    out << "\n";
    for (const auto& e : errors) {
        out << "[#" << e.timestamp << "] " << e.message << "\n";
    }
}
}  // namespace APBSystem