// src/main.cpp
#include "apb_analyzer.hpp"
#include "apb_types.hpp"  // 確保這個在最前面，或者至少在用到 APBSystem::SignalState 之前
#include "error_logger.hpp"
#include "report_generator.hpp"
#include "signal_manager.hpp"
#include "statistics.hpp"
#include "vcd_parser.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>  // For std::put_time in get_current_datetime_str
#include <iostream>
#include <string>

// 全域變數，用於在VCD解析回呼和主邏輯間傳遞當前VCD觀察到的訊號值
APBSystem::SignalState g_vcd_current_signal_values;  // 正確使用命名空間
bool g_previous_pclk_for_edge_detection = false;
int g_current_vcd_timestamp = 0;

std::string get_current_datetime_str() {
    auto t = std::time(nullptr);
    auto tm_struct = *std::localtime(&t);  // localtime返回指標，需要解引用
    std::ostringstream oss;
    oss << std::put_time(&tm_struct, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

int main(int argc, char* argv[]) {
    std::string vcd_filename = "pulpino_testcase1.vcd.txt";  // 預設檔名
    std::string output_filename = "output.txt";              // 預設檔名

    if (argc == 2) {  // 只提供輸入檔
        vcd_filename = argv[1];
    } else if (argc >= 4 && std::string(argv[2]) == "-o") {  // 提供輸入和輸出檔
        vcd_filename = argv[1];
        output_filename = argv[3];
    } else if (argc != 1) {  // 參數數量不符預期 (argc==1 表示使用預設)
        std::cerr << "Usage: " << argv[0] << " [<input.vcd>] [-o <output.txt>]\n";
        std::cerr << "If no arguments, defaults to: pulpino_testcase1.vcd.txt and output.txt\n";
        return 1;
    }
    // 如果 argc == 1，則使用預設檔名

    auto prog_start_time = std::chrono::high_resolution_clock::now();

    APBSystem::SignalManager signal_mgr;
    Statistics stats;
    ErrorLogger error_lgr;
    APBSystem::ApbAnalyzer analyzer(stats, error_lgr, signal_mgr);

    APBSystem::VcdParser vcd_parser;

    auto var_def_cb = [&](const std::string& id, const std::string& type_str, int width, const std::string& name) {
        signal_mgr.register_signal(id, type_str, width, name);
    };

    auto time_cb = [&](int time) {
        g_current_vcd_timestamp = time;
        analyzer.process_vcd_timestamp(time);
    };

    auto val_change_cb = [&](const std::string& id, const std::string& value_str) {
        bool pclk_rose = signal_mgr.update_signal_state_from_vcd(
            id, value_str,
            g_vcd_current_signal_values,  // 更新全域的訊號值
            g_previous_pclk_for_edge_detection);
        g_previous_pclk_for_edge_detection = g_vcd_current_signal_values.pclk;

        if (pclk_rose) {
            // 將最新的匯流排狀態傳遞給 analyzer 的事件處理函式
            analyzer.on_pclk_rising_edge(g_vcd_current_signal_values);
        }
    };

    auto end_def_cb = [&]() {
        analyzer.finalize_signal_definitions();
    };

    auto end_dumpvars_cb = [&]() {
        // 在 $dumpvars $end 之後，確保 g_previous_pclk_for_edge_detection
        // 反映了 $dumpvars 區段之後 PCLK 的初始狀態。
        g_previous_pclk_for_edge_detection = g_vcd_current_signal_values.pclk;
        // 如果初始PCLK ($dumpvars後第一個值) 就是1，且之前是0 (或未定義為0)，
        // 這裡可能需要觸發一次 on_pclk_rising_edge。
        // 但通常 $dumpvars 的值變更也會透過 val_change_cb 處理。
        // 這裡主要是確保邊緣偵測的基準正確。
    };

    std::cout << "Parsing VCD file: " << vcd_filename << "..." << std::endl;
    if (!vcd_parser.parse_file(vcd_filename, var_def_cb, time_cb, val_change_cb, end_def_cb, end_dumpvars_cb)) {
        std::cerr << "Failed to parse VCD file." << std::endl;
        // munmap 在 VcdParser 內部處理，如果 mmap 失敗會在內部 perror
        return 1;
    }
    std::cout << "VCD parsing completed." << std::endl;

    analyzer.finalize_analysis_and_fault_identification();
    std::cout << "Analysis finalized." << std::endl;

    error_lgr.sort_errors();

    auto prog_end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> cpu_time_ms = prog_end_time - prog_start_time;

    APBSystem::ReportGenerator report_gen;
    std::cout << "Generating report: " << output_filename << "..." << std::endl;
    report_gen.generate_report(output_filename, stats, error_lgr, analyzer, cpu_time_ms.count());
    std::cout << "Report generated successfully." << std::endl;

    return 0;
}