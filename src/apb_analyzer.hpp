// apb_analyzer.hpp
#pragma once

#include "apb_types.hpp"  // 包含 APBSystem::ApbFsmState, APBSystem::TransactionInfo, APBSystem::SignalState
#include "error_logger.hpp"
#include "statistics.hpp"

namespace APBSystem {

class ApbAnalyzer {
   public:
    ApbAnalyzer(Statistics& statistics, ErrorLogger& error_logger);

    // 在每個 PCLK 上升沿被調用
    // current_snapshot 包含了 PCLK 上升沿那個瞬間的所有 APB 訊號狀態
    // pclk_edge_count 是 PCLK 上升沿的計數 (從1開始)
    void analyze_on_pclk_rising_edge(const SignalState& current_snapshot, uint64_t pclk_edge_count);

    // VCD 分析結束時調用，用於處理任何未完成的交易或最終檢查
    void finalize_analysis(uint64_t final_vcd_timestamp_ps);

   private:
    bool m_system_out_of_reset;
    uint64_t m_first_valid_pclk_edge_for_stats;

    Statistics& m_statistics;
    ErrorLogger& m_error_logger;

    ApbFsmState m_current_apb_fsm_state;
    TransactionInfo m_current_transaction;  // 追蹤當前正在進行的交易
    uint64_t m_current_pclk_edge_count;     // 內部追蹤當前分析到的 PCLK 上升沿編號

    // 狀態處理輔助函式
    void handle_idle_state(const SignalState& snapshot);
    void handle_setup_state(const SignalState& snapshot);
    void handle_access_state(const SignalState& snapshot);

    // 檢查控制訊號是否有 'x'，並記錄錯誤 (可選)
    void check_control_signals_for_x(const SignalState& snapshot, uint64_t pclk_edge);
};

}  // namespace APBSystem