#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "apb_analyzer.hpp"
#include "apb_types.hpp"     // 包含 SignalState
#include "error_logger.hpp"  // 雖然主要目標是統計，但保留以備將來擴展錯誤偵測
#include "report_generator.hpp"
#include "signal_manager.hpp"
#include "statistics.hpp"
#include "vcd_parser.hpp"

// 使用定義的命名空間
using namespace APBSystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "用法: " << argv[0] << " <vcd_file_path>" << std::endl;
        return 1;
    }
    std::string vcd_file_path = argv[1];

    auto R_PROGRAM_START_TIME = std::chrono::high_resolution_clock::now();

    // 1. 實例化模組
    VcdParser vcd_parser;
    SignalManager signal_manager;  // SignalManager 現在主要用於註冊訊號和解析訊號值
    Statistics statistics;
    ErrorLogger error_logger;                            // 用於記錄可能的協議錯誤或數據 'x' 問題
    ApbAnalyzer apb_analyzer(statistics, error_logger);  // ApbAnalyzer 不再直接持有 SignalManager
    ReportGenerator report_generator;

    // 2. 準備共享狀態和回呼中使用的變數
    SignalState current_signal_snapshot;                // 這個物件會在 VCD 事件中被 SignalManager 更新
                                                        // 並在 PCLK 上升沿時傳遞給 ApbAnalyzer
    bool previous_pclk_val_for_edge_detection = false;  // 用於 SignalManager 檢測 PCLK 上升沿
                                                        // 初始值應與 VCD dumpvars 中 PCLK 的初始值一致 (通常是0)
                                                        // SignalState 構造函式中 pclk 預設為 false，所以這裡也 false
    uint64_t pclk_rising_edge_counter = 0;              // PCLK 上升沿的計數器
    uint64_t last_processed_vcd_timestamp = 0;          // 記錄 VCD 中最後處理的時間戳

    // 3. 設定 VcdParser 的回呼函式
    // 當 VcdParser 解析到 $var 定義時呼叫
    auto var_definition_callback =
        [&](const std::string& id_code, const std::string& type_str, int width, const std::string& hierarchical_name) {
            signal_manager.register_signal(id_code, type_str, width, hierarchical_name);
        };

    // 當 VcdParser 解析到 #timestamp 時呼叫
    auto timestamp_callback =
        [&](uint64_t vcd_time_ps) {  // VcdParser 的 TimestampCallback 應使用 uint64_t
            current_signal_snapshot.timestamp_ps = vcd_time_ps;
            last_processed_vcd_timestamp = vcd_time_ps;
        };

    // 當 VcdParser 解析到訊號值變化時呼叫
    auto value_change_callback =
        [&](const std::string& id_code, const std::string& value_str) {
            // SignalManager 更新 current_signal_snapshot 並檢測 PCLK 上升沿
            bool pclk_did_rise = signal_manager.update_state_on_signal_change(
                id_code, value_str, current_signal_snapshot, previous_pclk_val_for_edge_detection);

            if (pclk_did_rise) {
                pclk_rising_edge_counter++;
                // 在 PCLK 上升沿，將當前的訊號快照傳遞給 ApbAnalyzer 進行分析
                // current_signal_snapshot.timestamp_ps 此時應為 PCLK 上升沿發生的 VCD 時間戳
                apb_analyzer.analyze_on_pclk_rising_edge(current_signal_snapshot, pclk_rising_edge_counter);
            }
        };

    // VCD $enddefinitions 標記回呼 (可選)
    auto end_definitions_callback = []() {
        // std::cout << "Info: VCD $enddefinitions reached." << std::endl;
    };

    // VCD $dumpvars 區塊結束回呼 (可選, 通常在第一個 #timestamp 前)
    auto end_dumpvars_callback = []() {
        // std::cout << "Info: VCD $dumpvars section processed (initial values)." << std::endl;
    };

    // 4. 執行 VCD 解析
    std::cout << "Info: 開始解析 VCD 檔案: " << vcd_file_path << std::endl;
    if (!vcd_parser.parse_file(vcd_file_path,
                               var_definition_callback,
                               timestamp_callback,
                               value_change_callback,
                               end_definitions_callback,
                               end_dumpvars_callback)) {
        std::cerr << "錯誤: 解析 VCD 檔案失敗: " << vcd_file_path << std::endl;
        // error_logger.log_critical("VCD parsing failed for file: " + vcd_file_path); // 可選
        return 1;
    }
    std::cout << "Info: VCD 解析完成。" << std::endl;
    std::cout << "Info: 共偵測到 " << pclk_rising_edge_counter << " 個 PCLK 上升沿。" << std::endl;

    // 5. 設定總 PCLK 上升沿數量到 Statistics (用於計算 Bus Utilization 和 Idle Cycles)
    statistics.set_total_pclk_rising_edges(pclk_rising_edge_counter);

    // 6. 呼叫 ApbAnalyzer 的分析結束函式
    apb_analyzer.finalize_analysis(last_processed_vcd_timestamp);
    statistics.analyze_bus_shorts();
    std::cout << "Info: APB 交易分析完成。" << std::endl;

    // 7. 計算並設定 CPU 執行時間
    auto R_PROGRAM_END_TIME = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> ELAPSED_CPU_TIME_MS = R_PROGRAM_END_TIME - R_PROGRAM_START_TIME;
    statistics.set_cpu_elapsed_time_ms(ELAPSED_CPU_TIME_MS.count());

    // 8. 生成並輸出報表
    std::cout << "\nInfo: 生成統計報表...\n"
              << std::endl;
    report_generator.generate_apb_transaction_report(statistics, std::cout);

    std::cout << "\n程式執行完畢。" << std::endl;

    return 0;
}