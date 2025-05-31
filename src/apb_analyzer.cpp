#include "apb_analyzer.hpp"
#include <iostream>  // 用於調試 (可選)

namespace APBSystem {

ApbAnalyzer::ApbAnalyzer(Statistics& statistics, ErrorLogger& error_logger)
    : m_statistics(statistics),
      m_error_logger(error_logger),
      m_current_apb_fsm_state(ApbFsmState::IDLE),
      m_current_pclk_edge_count(0),
      m_system_out_of_reset(false),
      m_first_valid_pclk_edge_for_stats(0) {
    m_current_transaction.reset();
}

void ApbAnalyzer::check_control_signals_for_x(const SignalState& snapshot, uint64_t pclk_edge) {
    // 根據題目要求，這裡可以添加對 PSEL, PENABLE, PWRITE, PREADY (如果來自 Master 側) 等控制訊號 'x' 狀態的檢查和錯誤記錄
    // 例如:
    // if (snapshot.psel_has_x) {
    //     m_error_logger.log_control_signal_x_error(pclk_edge, snapshot.timestamp_ps, "PSEL");
    // }
    // if (snapshot.pwrite_has_x && snapshot.psel) { // PWRITE 只有在 PSEL 有效時才重要
    //     m_error_logger.log_control_signal_x_error(pclk_edge, snapshot.timestamp_ps, "PWRITE");
    // }
    // ...等等
}

void ApbAnalyzer::analyze_on_pclk_rising_edge(const SignalState& current_snapshot, uint64_t pclk_edge_count) {
    m_current_pclk_edge_count = pclk_edge_count;

    if (!m_system_out_of_reset) {
        if (current_snapshot.presetn) {  // 如果 presetn 為高，系統已經出 Reset
            m_system_out_of_reset = true;
            m_first_valid_pclk_edge_for_stats = pclk_edge_count;  // 記錄第一個有效的 PCLK 上升沿
        } else {
            return;  // 如果還在 Reset 狀態，則不進行任何分析
        }
    }
    // 1. 記錄總線活動 (如果 PSEL 為高，則此 PCLK 上升沿計為活動)
    if (current_snapshot.psel && !current_snapshot.psel_has_x) {  // 只有當 PSEL 確定為高時
        m_statistics.record_bus_active_pclk_edge();
    }

    // 2. (可選) 檢查重要控制訊號是否有 'x'
    // check_control_signals_for_x(current_snapshot, pclk_edge_count);

    // 3. APB 狀態機邏輯
    // 注意：所有訊號值都從 current_snapshot 中獲取
    switch (m_current_apb_fsm_state) {
        case ApbFsmState::IDLE:
            handle_idle_state(current_snapshot);
            break;
        case ApbFsmState::SETUP:
            handle_setup_state(current_snapshot);
            break;
        case ApbFsmState::ACCESS:
            handle_access_state(current_snapshot);
            break;
    }
}

void ApbAnalyzer::handle_idle_state(const SignalState& snapshot) {
    if (snapshot.psel && !snapshot.psel_has_x) {  // PSEL 為高且確定
        m_current_apb_fsm_state = ApbFsmState::SETUP;
        m_current_transaction.active = true;
        m_current_transaction.start_pclk_edge_count = m_current_pclk_edge_count;
        m_current_transaction.transaction_start_time_ps = snapshot.timestamp_ps;     // 記錄交易開始的 VCD 時間
        m_current_transaction.is_write = snapshot.pwrite && !snapshot.pwrite_has_x;  // 如果 pwrite_has_x，按讀處理或報錯
        m_current_transaction.paddr = snapshot.paddr;                                // PADDR 的 'x' 狀態由 Statistics::record_completer_access 內部或 ErrorLogger 處理
        m_current_transaction.had_wait_state = false;

        if (snapshot.paddr_has_x) {
            // m_error_logger.log_address_x_error(m_current_pclk_edge_count, snapshot.timestamp_ps, "PADDR contains 'x' at transaction start.");
        }
        if (snapshot.pwrite_has_x && snapshot.psel) {  // PWRITE 為 'x' 且 PSEL 有效
                                                       // m_error_logger.log_control_signal_x_error(m_current_pclk_edge_count, snapshot.timestamp_ps, "PWRITE");
                                                       // 可以決定 PWRITE 為 'x' 時交易如何處理，例如視為讀，或視為錯誤交易不計入統計
        }

        if (snapshot.penable && !snapshot.penable_has_x) {  // PENABLE 在 IDLE->SETUP 轉換時不應為高
            // m_error_logger.log_protocol_error(m_current_pclk_edge_count, snapshot.timestamp_ps, "PENABLE is high during IDLE to SETUP transition.");
        }
    }
    // 若 PSEL 仍為低或為 'x'，則保持在 IDLE
}

void ApbAnalyzer::handle_setup_state(const SignalState& snapshot) {
    if (!snapshot.psel || snapshot.psel_has_x) {  // PSEL 掉低或變為 'x'，異常終止
        if (m_current_transaction.active) {
            // m_error_logger.log_transaction_aborted_error(m_current_pclk_edge_count, snapshot.timestamp_ps, m_current_transaction.paddr, "PSEL deasserted or became 'x' during SETUP.");
            m_current_transaction.reset();
        }
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }

    // PSEL 保持為高
    if (snapshot.penable && !snapshot.penable_has_x) {  // PENABLE 為高且確定，進入 ACCESS
        m_current_apb_fsm_state = ApbFsmState::ACCESS;
        // 在進入 ACCESS 階段時記錄 Completer 存取
        if (!snapshot.paddr_has_x) {  // 只有當地址確定時才記錄
            m_statistics.record_completer_access(m_current_transaction.paddr);
        } else {
            // m_error_logger.log_address_x_error(m_current_pclk_edge_count, snapshot.timestamp_ps, "PADDR is 'x' when entering ACCESS phase.");
            // 此交易可能因地址未知而不計入某些統計，或被標記為錯誤
        }

        if (!snapshot.pready && !snapshot.pready_has_x) {  // PREADY 為低且確定，表示有等待
            m_current_transaction.had_wait_state = true;
        } else if (snapshot.pready && !snapshot.pready_has_x) {  // PREADY 為高且確定，交易在此 PCLK 上升沿完成
            uint64_t duration_edges = m_current_pclk_edge_count - m_current_transaction.start_pclk_edge_count + 1;
            // APB 最小交易是 2 個 PCLK 週期 (SETUP -> ACCESS)
            if (duration_edges < 2 && m_current_pclk_edge_count >= m_current_transaction.start_pclk_edge_count) {
                duration_edges = 2;  // 強制為2個PCLK edge
            }

            if (m_current_transaction.is_write) {
                if (snapshot.pwdata_has_x && m_current_transaction.active) { /* 記錄寫數據 'x' 錯誤 */
                }
                m_statistics.record_write_transaction(m_current_transaction.had_wait_state, duration_edges);
            } else {
                if (snapshot.prdata_has_x && m_current_transaction.active) { /* 記錄讀數據 'x' 錯誤 */
                }
                m_statistics.record_read_transaction(m_current_transaction.had_wait_state, duration_edges);
            }
            m_current_transaction.reset();
            m_current_apb_fsm_state = ApbFsmState::IDLE;
        } else if (snapshot.pready_has_x) {  // PREADY 為 'x'
            // m_error_logger.log_control_signal_x_error(m_current_pclk_edge_count, snapshot.timestamp_ps, "PREADY");
            // 如何處理 PREADY 為 'x'？可以視為等待，或視為錯誤並中止交易
            m_current_transaction.had_wait_state = true;  // 保守地視為等待，但可能需要更嚴格的錯誤處理
        }
    }
    // 若 PENABLE 仍為低 (或為 'x')，則保持在 SETUP 狀態
    else if (snapshot.penable_has_x && snapshot.psel && !snapshot.psel_has_x) {
        // m_error_logger.log_control_signal_x_error(m_current_pclk_edge_count, snapshot.timestamp_ps, "PENABLE");
        // PENABLE 為 'x' 時，狀態機行為未定義，可以保持 SETUP 或報錯中止
    }
}

void ApbAnalyzer::handle_access_state(const SignalState& snapshot) {
    if (!snapshot.psel || snapshot.psel_has_x) {  // PSEL 掉低或變為 'x'
        if (m_current_transaction.active) {
            // m_error_logger.log_transaction_aborted_error(m_current_pclk_edge_count, snapshot.timestamp_ps, m_current_transaction.paddr, "PSEL deasserted or became 'x' during ACCESS.");
            m_current_transaction.reset();
        }
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }
    if (!snapshot.penable || snapshot.penable_has_x) {  // PENABLE 意外掉低或變為 'x'
        if (m_current_transaction.active) {
            // m_error_logger.log_protocol_error(m_current_pclk_edge_count, snapshot.timestamp_ps, "PENABLE deasserted or became 'x' during ACCESS before PREADY.");
            m_current_transaction.reset();
        }
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }

    // PSEL 和 PENABLE 均保持為高且確定
    if (snapshot.pready && !snapshot.pready_has_x) {  // PREADY 為高且確定，交易完成
        uint64_t duration_edges = m_current_pclk_edge_count - m_current_transaction.start_pclk_edge_count + 1;
        if (duration_edges < 2 && m_current_pclk_edge_count >= m_current_transaction.start_pclk_edge_count) {
            duration_edges = 2;
        }

        if (m_current_transaction.is_write) {
            if (snapshot.pwdata_has_x && m_current_transaction.active) { /* 記錄寫數據 'x' 錯誤 */
            }
            m_statistics.record_write_transaction(m_current_transaction.had_wait_state, duration_edges);
        } else {
            if (snapshot.prdata_has_x && m_current_transaction.active) { /* 記錄讀數據 'x' 錯誤 */
            }
            m_statistics.record_read_transaction(m_current_transaction.had_wait_state, duration_edges);
        }
        m_current_transaction.reset();
        m_current_apb_fsm_state = ApbFsmState::IDLE;
    } else if (!snapshot.pready && !snapshot.pready_has_x) {  // PREADY 為低且確定，保持等待
        m_current_transaction.had_wait_state = true;
    } else if (snapshot.pready_has_x) {  // PREADY 為 'x'
        // m_error_logger.log_control_signal_x_error(m_current_pclk_edge_count, snapshot.timestamp_ps, "PREADY");
        m_current_transaction.had_wait_state = true;  // 保守處理，視為等待
    }
}

void ApbAnalyzer::finalize_analysis(uint64_t final_vcd_timestamp_ps) {
    if (m_current_transaction.active) {
        // m_error_logger.log_transaction_aborted_error(m_current_pclk_edge_count, final_vcd_timestamp_ps, m_current_transaction.paddr,
        //                                   "Transaction was still active at the end of VCD analysis.");
        m_current_transaction.reset();
    }
    m_current_apb_fsm_state = ApbFsmState::IDLE;

    // 可以進行一些最終的統計數據一致性檢查等
    m_statistics.set_first_valid_pclk_edge_for_stats(m_first_valid_pclk_edge_for_stats);
}
}  // namespace APBSystem