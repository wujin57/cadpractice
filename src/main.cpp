#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "apb_analyzer.hpp"
#include "apb_types.hpp"
#include "report_generator.hpp"
#include "signal_manager.hpp"
#include "statistics.hpp"
#include "vcd_parser.hpp"

using namespace APBSystem;

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <input_vcd_file> -o <output_txt_file>" << std::endl;
        return 1;
    }
    std::string vcd_file_path = argv[1];
    std::string output_file_path = argv[3];
    std::ofstream out_file(output_file_path);
    if (!out_file.is_open()) {
        std::cerr << "Error: Could not open output file: " << output_file_path << std::endl;
        return 1;
    }

    // 為了除錯，我們依然保留日誌檔案
    std::ofstream debug_log_file("debug_log.txt");
    if (!debug_log_file.is_open()) {
        std::cerr << "Warning: Could not open debug_log.txt for writing." << std::endl;
    }

    auto R_PROGRAM_START_TIME = std::chrono::high_resolution_clock::now();

    VcdParser vcd_parser;
    SignalManager signal_manager;
    Statistics statistics;
    ApbAnalyzer apb_analyzer(statistics, debug_log_file);
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
        [&](char id_char, const char* value_ptr, size_t value_len) {
            bool pclk_did_rise = signal_manager.update_state_on_signal_change(
                id_char, value_ptr, value_len,
                current_signal_snapshot,
                previous_pclk_val_for_edge_detection);

            if (pclk_did_rise) {
                pclk_rising_edge_counter++;
                apb_analyzer.analyze_on_pclk_rising_edge(current_signal_snapshot, pclk_rising_edge_counter);
            }
        };

    // MODIFIED: 這是本次修正的核心！
    // 我們在 VCD 解析器讀完所有變數定義 ($enddefinitions) 後，
    // 立刻設定一次匯流排的寬度。
    auto end_definitions_callback = [&]() {
        statistics.set_bus_widths(signal_manager.get_paddr_width(), signal_manager.get_pwdata_width());
    };

    auto end_dumpvars_callback = []() {};

    if (!vcd_parser.parse_file(vcd_file_path,
                               var_definition_callback,
                               timestamp_callback,
                               value_change_callback,
                               end_definitions_callback  // 將我們新的回呼函式傳入
                               )) {
        std::cerr << "錯誤: 解析 VCD 檔案失敗: " << vcd_file_path << std::endl;
        out_file.close();
        debug_log_file.close();
        return 1;
    }

    // 這裡就不再需要呼叫 set_bus_widths，因為在解析過程中已經做過了。
    statistics.set_total_pclk_rising_edges(pclk_rising_edge_counter);

    apb_analyzer.finalize_analysis(last_processed_vcd_timestamp);

    // 執行最終的位元活動分析
    statistics.finalize_bit_activity();

    auto R_PROGRAM_END_TIME = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ELAPSED_CPU_TIME_MS = R_PROGRAM_END_TIME - R_PROGRAM_START_TIME;
    statistics.set_cpu_elapsed_time_ms(ELAPSED_CPU_TIME_MS.count());

    report_generator.generate_apb_transaction_report(statistics, out_file);

    out_file.close();
    debug_log_file.close();

    return 0;
}
