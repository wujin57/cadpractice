// report_generator.cpp
#include "report_generator.hpp"
#include <iomanip>   // For std::setw, std::left
#include <iostream>  // For std::cerr
#include <vector>

namespace APBSystem {

void ReportGenerator::write_completer_connection_report(std::ofstream& outfile,
                                                        int completer_idx_0_based,
                                                        const IdentifiedFloatingPair& paddr_fault,
                                                        int paddr_bus_width,
                                                        const IdentifiedFloatingPair& pwdata_fault,
                                                        int pwdata_bus_width) {
    outfile << "Completer " << (completer_idx_0_based + 1) << " PADDR Connections ("
            << "a" << PADDR_REPORT_BITS - 1 << " to a0):" << std::endl;
    for (int i = 0; i < PADDR_REPORT_BITS; ++i) {  // Report PADDR_REPORT_BITS (e.g., 8 bits)
        std::string status = "Correct";
        if (paddr_fault.isActive) {
            if (i == paddr_fault.bit1)
                status = "Connected with a" + std::to_string(paddr_fault.bit2);
            else if (i == paddr_fault.bit2)
                status = "Connected with a" + std::to_string(paddr_fault.bit1);
        }
        outfile << "  a" << i << ": " << status << std::endl;
    }
    outfile << std::endl;

    outfile << "Completer " << (completer_idx_0_based + 1) << " PWDATA Connections ("
            << "d" << PWDATA_REPORT_BITS - 1 << " to d0):" << std::endl;
    for (int i = 0; i < PWDATA_REPORT_BITS; ++i) {  // Report PWDATA_REPORT_BITS
        std::string status = "Correct";
        if (pwdata_fault.isActive) {
            if (i == pwdata_fault.bit1)
                status = "Connected with d" + std::to_string(pwdata_fault.bit2);
            else if (i == pwdata_fault.bit2)
                status = "Connected with d" + std::to_string(pwdata_fault.bit1);
        }
        outfile << "  d" << i << ": " << status << std::endl;
    }
    outfile << std::endl;
}

void ReportGenerator::generate_report(const std::string& output_filename,
                                      const Statistics& stats,
                                      const ErrorLogger& error_logger,  // Assume errors are sorted
                                      const ApbAnalyzer& analyzer,
                                      double cpu_elapsed_time_ms) {
    std::ofstream outfile(output_filename);
    if (!outfile.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_filename << std::endl;
        return;
    }

    outfile << "APB Transaction Recognizer Report" << std::endl;
    outfile << "---------------------------------" << std::endl;
    outfile << "VCD File: <Input VCD Filename Here - main.cpp should pass this>" << std::endl;  // Placeholder
    outfile << "Generated on: <Date and Time Here - main.cpp can generate this>" << std::endl;  // Placeholder
    outfile << "---------------------------------" << std::endl
            << std::endl;

    outfile << "I. Transaction Statistics:" << std::endl;
    outfile << "  Read Transactions (No Wait): " << stats.get_read_no_wait() << std::endl;
    outfile << "  Read Transactions (With Wait): " << stats.get_read_wait() << std::endl;
    outfile << "  Write Transactions (No Wait): " << stats.get_write_no_wait() << std::endl;
    outfile << "  Write Transactions (With Wait): " << stats.get_write_wait() << std::endl;
    outfile << "  Average Read Cycle: " << stats.get_avg_read_cycle_str() << std::endl;
    outfile << "  Average Write Cycle: " << stats.get_avg_write_cycle_str() << std::endl;
    outfile << "  Bus Utilization: " << stats.get_bus_utilization_str() << "%" << std::endl;
    outfile << "  Number of Idle Cycles: " << stats.get_total_idle_cycles() << std::endl;
    outfile << "  Number of Completers: " << stats.get_num_completers() << std::endl;
    outfile << std::endl;

    outfile << "II. Error Summary:" << std::endl;
    outfile << "  Address Corruption: " << stats.get_error_count("AddressCorruption") << std::endl;
    outfile << "  Data Corruption: " << stats.get_error_count("DataCorruption") << std::endl;
    outfile << "  Out-of-Range Access: " << stats.get_error_count("OutOfRangeAccess") << std::endl;
    // outfile << "  Data Mirroring: " << stats.get_error_count("DataMirroring") << std::endl; // Add if implemented
    outfile << "  Read-Write Overlap: " << stats.get_error_count("ReadWriteOverlap") << std::endl;
    outfile << "  Transaction Timeout: " << stats.get_error_count("Timeout") << std::endl;
    outfile << "  PSLVERR Occurrences: " << stats.get_error_count("PSLVERR") << std::endl;
    outfile << "  PADDR Instability: " << stats.get_error_count("PADDRInstability") << std::endl;
    outfile << "  PWDATA Instability: " << stats.get_error_count("PWDATAInstability") << std::endl;
    // Add counts for other specific protocol errors if you track them
    outfile << std::endl;

    outfile << "III. Completer PADDR/PWDATA Connections:" << std::endl;
    int num_completers_to_report = std::min(stats.get_num_completers(), MAX_COMPLETERS);
    if (num_completers_to_report == 0 && (analyzer.get_paddr_fault_info(0).isActive || analyzer.get_pwdata_fault_info(0).isActive)) {
        // If no transactions completed to define num_completers but faults were found for completer 0 (default)
        num_completers_to_report = 1;
    }

    for (int i = 0; i < num_completers_to_report; ++i) {
        write_completer_connection_report(outfile, i,
                                          analyzer.get_paddr_fault_info(i), analyzer.get_paddr_bus_width(),
                                          analyzer.get_pwdata_fault_info(i), analyzer.get_pwdata_bus_width());
    }
    if (num_completers_to_report == 0) {
        outfile << "  No active completers identified or no PADDR/PWDATA faults detected for default completers." << std::endl
                << std::endl;
    }

    outfile << "IV. Detailed Error Log (Chronological):" << std::endl;
    const auto& errors = error_logger.get_errors();  // Assume sorted by error_logger
    if (errors.empty()) {
        outfile << "  No errors detected." << std::endl;
    } else {
        for (const auto& err_info : errors) {
            outfile << "  [#" << err_info.timestamp << "] " << err_info.message << std::endl;
        }
    }
    outfile << std::endl;

    outfile << "V. Performance:" << std::endl;
    outfile << "  CPU Elapsed Time: " << std::fixed << std::setprecision(2) << cpu_elapsed_time_ms << " ms" << std::endl;
    outfile << "---------------------------------" << std::endl;
    outfile << "End of Report" << std::endl;

    outfile.close();
}

}  // namespace APBSystem