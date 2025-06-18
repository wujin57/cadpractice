#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "apb_analyzer.hpp"
#include "apb_types.hpp"
#include "error_logger.hpp"
#include "report_generator.hpp"
#include "signal_manager.hpp"
#include "statistics.hpp"
#include "vcd_parser.hpp"

// 使用定義的命名空間
using namespace APBSystem;

int main(int argc, char* argv[]) {
    std::string vcd_file_path = argv[1];
    auto R_PROGRAM_START_TIME = std::chrono::high_resolution_clock::now();

    VcdParser vcd_parser;
    SignalManager signal_manager;
    Statistics statistics;
    ErrorLogger error_logger;
    ApbAnalyzer apb_analyzer(statistics, error_logger);
    ReportGenerator report_generator;

    SignalState current_signal_snapshot;

    bool previous_pclk_val_for_edge_detection = false;
    uint64_t pclk_rising_edge_counter = 0;
    uint64_t last_processed_vcd_timestamp = 0;

    auto var_definition_callback =
        [&](const std::string& id_code, const std::string& type_str, int width, const std::string& hierarchical_name) {
            signal_manager.register_signal(id_code, type_str, width, hierarchical_name);
        };

    auto timestamp_callback =
        [&](uint64_t vcd_time_ps) {
            current_signal_snapshot.timestamp_ps = vcd_time_ps;
            last_processed_vcd_timestamp = vcd_time_ps;
        };

    auto value_change_callback =
        [&](const std::string& id_code, const std::string& value_str) {
            bool pclk_did_rise = signal_manager.update_state_on_signal_change(
                id_code, value_str, current_signal_snapshot, previous_pclk_val_for_edge_detection);

            if (pclk_did_rise) {
                pclk_rising_edge_counter++;
                apb_analyzer.analyze_on_pclk_rising_edge(current_signal_snapshot, pclk_rising_edge_counter);
            }
        };

    auto end_definitions_callback = []() {
    };

    auto end_dumpvars_callback = []() {
    };

    if (!vcd_parser.parse_file(vcd_file_path,
                               var_definition_callback,
                               timestamp_callback,
                               value_change_callback,
                               end_definitions_callback,
                               end_dumpvars_callback)) {
        std::cerr << "錯誤: 解析 VCD 檔案失敗: " << vcd_file_path << std::endl;
        return 1;
    }

    statistics.set_bus_widths(signal_manager.get_paddr_width(), signal_manager.get_pwdata_width());

    statistics.set_total_pclk_rising_edges(pclk_rising_edge_counter);

    // 6. 呼叫 ApbAnalyzer 的分析結束函式
    apb_analyzer.finalize_analysis(last_processed_vcd_timestamp);

    auto R_PROGRAM_END_TIME = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ELAPSED_CPU_TIME_MS = R_PROGRAM_END_TIME - R_PROGRAM_START_TIME;
    statistics.set_cpu_elapsed_time_ms(ELAPSED_CPU_TIME_MS.count());

    std::string output_file_path = vcd_file_path;
    size_t dot_pos = output_file_path.rfind('.');
    size_t slash_pos = output_file_path.find_last_of("/\\");

    if (dot_pos != std::string::npos && (slash_pos == std::string::npos || dot_pos > slash_pos)) {
        output_file_path.replace(dot_pos, std::string::npos, ".txt");
    } else {
        output_file_path += ".txt";
    }

    std::ofstream output_file(output_file_path);
    if (!output_file.is_open()) {
        std::cerr << "錯誤: 無法開啟輸出檔案: " << output_file_path << std::endl;
        return 1;
    }

    report_generator.generate_apb_transaction_report(statistics, output_file);
    output_file.close();
    return 0;
}