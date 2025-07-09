// report_generator.cpp
#include "report_generator.hpp"
#include <algorithm>
#include <iomanip>
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
    out << "1. Number of Read Transactions with no wait states: " << stats.get_read_transactions_no_wait() << "\n";
    out << "2. Number of Read Transactions with wait states: " << stats.get_read_transactions_with_wait() << "\n";
    out << "3. Number of Write Transactions with no wait states: " << stats.get_write_transactions_no_wait() << "\n";
    out << "4. Number of Write Transactions with wait states: " << stats.get_write_transactions_with_wait() << "\n";
    out << std::fixed << std::setprecision(2);
    out << "5. Average Read Cycle: " << stats.get_average_read_cycle_duration() << " cycles\n";
    out << "6. Average Write Cycle: " << stats.get_average_write_cycle_duration() << " cycles\n";
    out << "7. Bus Utilization: " << stats.get_bus_utilization_percentage() << "%\n";
    out << std::defaultfloat << std::setprecision(0);
    out << "8. Number of Idle Cycles: " << stats.get_num_idle_pclk_edges() << "\n";
    out << "9. Number of Completer: " << stats.get_number_of_unique_completers_accessed() << "\n";
    out << std::fixed << std::setprecision(2);
    out << "10. CPU Elapsed Time: " << stats.get_cpu_elapsed_time_ms() << " ms\n";
    out << std::defaultfloat << std::setprecision(6);

    // Section 2: Error Summary
    out << "\nNumber of Transactions with Timeout: " << stats.get_timeout_error_details().size() << "\n";
    out << "Number of Out-of-Range Accesses: " << stats.get_out_of_range_details().size() << "\n";
    out << "Number of Mirrored Transactions: " << stats.get_mirroring_error_count() << "\n";
    out << "Number of Read-Write Overlap Errors: " << stats.get_read_write_overlap_details().size() << "\n";

    // Section 3: Completer Connection Status
    const auto& completers = stats.get_ordered_accessed_completers();
    const auto& activity_map = stats.get_completer_bit_activity_map();
    for (int i = 0; i < completers.size(); ++i) {
        const auto& cid = completers[i];
        out << "\nCompleter " << (i + 1) << " PADDR Connections\n";
        if (activity_map.count(cid)) {
            const auto& paddr_details = activity_map.at(cid).paddr_bit_details;
            for (int j = paddr_details.size() - 1; j >= 0; --j) {
                out << "a" << std::setw(2) << std::setfill('0') << j << ": " << bit_detail_status_to_report_string(paddr_details[j], 'a') << "\n";
            }
        }
        out << "\nCompleter " << (i + 1) << " PWDATA Connections\n";
        if (activity_map.count(cid)) {
            const auto& pwdata_details = activity_map.at(cid).pwdata_bit_details;
            for (int j = pwdata_details.size() - 1; j >= 0; --j) {
                out << "d" << std::setw(2) << std::setfill('0') << j << ": " << bit_detail_status_to_report_string(pwdata_details[j], 'd') << "\n";
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
        errors.push_back({d.timestamp_ps, "Out-of-Range Access -> PADDR 0x" + (std::stringstream() << std::hex << d.paddr).str()});
    }
    for (const auto& d : stats.get_timeout_error_details()) {
        errors.push_back({d.start_timestamp_ps, "Timeout Occurred -> Transaction Stalled at PADDR 0x" + (std::stringstream() << std::hex << d.paddr).str()});
    }
    for (const auto& d : stats.get_read_write_overlap_details()) {
        errors.push_back({d.timestamp_ps, "Read-Write Overlap Error -> Read & Write at PADDR 0x" + (std::stringstream() << std::hex << d.paddr).str() + " overlapped"});
    }
    for (const auto& d : stats.get_data_mirroring_details()) {
        errors.push_back({d.original_write_time, "Address Mirroring -> Write at PADDR 0x" + (std::stringstream() << std::hex << d.original_write_addr).str() + " also reflected at PADDR 0x" + (std::stringstream() << std::hex << d.mirrored_addr).str()});
        errors.push_back({d.read_timestamp, "Data Mirroring -> Value 0x" + (std::stringstream() << std::hex << d.data_value).str() + " written at PADDR 0x" + (std::stringstream() << std::hex << d.original_write_addr).str() + " also found at PADDR 0x" + (std::stringstream() << std::hex << d.mirrored_addr).str()});
    }
    for (const auto& d : stats.get_address_corruption_details()) {
        std::ostringstream oss;
        oss << "Address Corruption -> Expected PADDR: 0x" << std::hex << d.corrupted_addr << ", Received: 0x" << d.corrupted_addr << " (a" << d.bit_a << "-a" << d.bit_b << " Floating)";
        errors.push_back({d.timestamp, oss.str()});
    }
    for (const auto& d : stats.get_data_corruption_details()) {
        std::ostringstream oss;
        oss << "Data Corruption -> Expected PWDATA: 0x" << std::hex << d.corrupted_pwdata << ", Received: 0x" << d.corrupted_pwdata << " (d" << d.bit_a << "-d" << d.bit_b << " Floating)";
        errors.push_back({d.timestamp, oss.str()});
    }

    std::sort(errors.begin(), errors.end());
    out << "\n";
    for (const auto& e : errors) {
        out << "[#" << e.timestamp << "] " << e.message << "\n";
    }
}
}  // namespace APBSystem