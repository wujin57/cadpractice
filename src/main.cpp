// main.cpp
#include "apb_analyzer.hpp"
#include "apb_types.hpp"  // For SignalState
#include "error_logger.hpp"
#include "report_generator.hpp"
#include "signal_manager.hpp"
#include "statistics.hpp"
#include "vcd_parser.hpp"

#include <chrono>
#include <ctime>    // For date/time in report
#include <iomanip>  // For put_time
#include <iostream>
#include <string>

// --- Global APB Signal States ---
// These are effectively the "current" state of the bus as driven by VCD updates.
// ApbAnalyzer will have its own internal current and previous copies for its PCLK-synchronous logic.
APBSystem::SignalState g_vcd_current_signal_values;  // Updated by SignalManager from VCD
bool g_previous_pclk_for_edge_detection = false;     // Managed by main loop
int g_current_vcd_timestamp = 0;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <input.vcd> -o <output.txt>\n";
        // Fallback for easier testing if args not provided:
        if (argc == 1) {
            std::cerr << "Development mode: Using default filenames: input.vcd, output.txt\n";
        } else {
            return 1;
        }
    }

    std::string vcd_filename = (argc >= 2) ? argv[1] : "input.vcd";  // Default for testing
    std::string output_filename = (argc >= 4 && std::string(argv[2]) == "-o") ? argv[3] : "output.txt";

    auto prog_start_time = std::chrono::high_resolution_clock::now();

    // 1. Instantiate modules
    APBSystem::SignalManager signal_mgr;
    Statistics stats;
    ErrorLogger error_lgr;
    APBSystem::ApbAnalyzer analyzer(stats, error_lgr, signal_mgr);  // Pass references

    APBSystem::VcdParser vcd_parser;

    // 2. Define VCD Parser Callbacks
    auto var_def_cb = [&](const std::string& id, const std::string& type_str, int width, const std::string& name) {
        signal_mgr.register_signal(id, type_str, width, name);
    };

    auto time_cb = [&](int time) {
        g_current_vcd_timestamp = time;
        analyzer.process_vcd_timestamp(time);  // Inform analyzer of current VCD time
    };

    auto val_change_cb = [&](const std::string& id, const std::string& value_str) {
        bool pclk_rose = signal_mgr.update_signal_state_from_vcd(id, value_str,
                                                                 g_vcd_current_signal_values,
                                                                 g_previous_pclk_for_edge_detection);
        g_previous_pclk_for_edge_detection = g_vcd_current_signal_values.pclk;  // Update for next VCD event

        if (pclk_rose) {
            // Pass a const reference to the most up-to-date signal values to the analyzer
            // The analyzer will internally copy this to its current_signal_state_ and manage its prev_signal_state_
            analyzer.current_signal_state_ = g_vcd_current_signal_values;  // Update analyzer's view
            analyzer.on_pclk_rising_edge();
        }
    };

    auto end_def_cb = [&]() {
        // std::cout << "VCD $enddefinitions reached." << std::endl;
        analyzer.finalize_signal_definitions();  // Analyzer can now get final bus widths etc.
    };
    auto end_dumpvars_cb = [&]() {
        // std::cout << "VCD $dumpvars $end reached." << std::endl;
        // After initial values from $dumpvars are processed by val_change_cb,
        // if PCLK was set, it should trigger on_pclk_rising_edge.
        // We might need to explicitly set the initial g_previous_pclk_for_edge_detection
        // based on the very first PCLK state after dumpvars if it's not 0.
        // For now, assume PCLK starts at 0 or val_change_cb handles first edge.
        g_previous_pclk_for_edge_detection = g_vcd_current_signal_values.pclk;
    };

    // 3. Parse the VCD file
    std::cout << "Parsing VCD file: " << vcd_filename << "..." << std::endl;
    if (!vcd_parser.parse_file(vcd_filename, var_def_cb, time_cb, val_change_cb, end_def_cb, end_dumpvars_cb)) {
        std::cerr << "Failed to parse VCD file." << std::endl;
        return 1;
    }
    std::cout << "VCD parsing completed." << std::endl;

    // 4. Finalize analysis (e.g., identify permanent faults)
    analyzer.finalize_analysis_and_fault_identification();
    std::cout << "Analysis finalized." << std::endl;

    // 5. Sort errors before reporting
    error_lgr.sort_errors();

    // 6. Generate Report
    auto prog_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> cpu_time_ms = prog_end_time - prog_start_time;

    APBSystem::ReportGenerator report_gen;
    std::cout << "Generating report: " << output_filename << "..." << std::endl;

    // Pass necessary info to report generator
    // The ReportGenerator needs the VCD filename for the report header
    // and current date/time
    report_gen.generate_report(output_filename, stats, error_lgr, analyzer, cpu_time_ms.count());
    std::cout << "Report generated successfully." << std::endl;

    return 0;
}