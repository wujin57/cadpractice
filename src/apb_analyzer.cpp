// apb_analyzer.cpp
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

CompleterID ApbAnalyzer::get_completer_id_from_paddr(uint32_t paddr) const {
    if (paddr >= UART_BASE_ADDR && paddr <= UART_END_ADDR) {
        return CompleterID::UART;
    } else if (paddr >= GPIO_BASE_ADDR && paddr <= GPIO_END_ADDR) {
        return CompleterID::GPIO;
    } else if (paddr >= SPI_MASTER_BASE_ADDR && paddr <= SPI_MASTER_END_ADDR) {
        return CompleterID::SPI_MASTER;
    }
    return CompleterID::UNKNOWN_COMPLETER;
}

void ApbAnalyzer::check_for_out_of_range(const SignalState& snapshot_at_completion) {
    if (!m_current_transaction.active || m_current_transaction.paddr_val_has_x) {
        m_current_transaction.is_out_of_range = true;
        return;
    }

    bool is_oor = false;
    CompleterID target_comp = m_current_transaction.target_completer;
    uint32_t current_paddr = m_current_transaction.paddr;

    switch (target_comp) {
        case CompleterID::UART:
            if (current_paddr < UART_BASE_ADDR || current_paddr > UART_END_ADDR) {
                is_oor = true;
            }
            break;
        case CompleterID::GPIO:
            if (current_paddr < GPIO_BASE_ADDR || current_paddr > GPIO_END_ADDR) {
                is_oor = true;
            }
            break;
        case CompleterID::SPI_MASTER:
            if (current_paddr < SPI_MASTER_BASE_ADDR || current_paddr > SPI_MASTER_END_ADDR) {
                is_oor = true;
            }
            break;
        case CompleterID::UNKNOWN_COMPLETER:
            is_oor = true;  // 未知的 Completer ID，視為 OOR
            break;
        case CompleterID::NONE:
            is_oor = true;  // 沒有指定 Completer ID，視為 OOR
            break;
    }

    m_current_transaction.is_out_of_range = is_oor;

    if (is_oor) {
        bool prdata_x_for_oor_read = !m_current_transaction.is_write && snapshot_at_completion.prdata_has_x;
        m_statistics.record_out_of_range_access({snapshot_at_completion.timestamp_ps,
                                                 m_current_transaction.paddr,
                                                 m_current_transaction.target_completer,
                                                 m_current_transaction.is_write,
                                                 prdata_x_for_oor_read});
    }
}

void ApbAnalyzer::analyze_on_pclk_rising_edge(const SignalState& current_snapshot, uint64_t pclk_edge_count) {
    m_current_pclk_edge_count = pclk_edge_count;

    if (!m_system_out_of_reset) {
        if (current_snapshot.presetn) {
            m_system_out_of_reset = true;
            m_first_valid_pclk_edge_for_stats = pclk_edge_count;
        } else {
            return;
        }
    }
    if (check_for_timeout(current_snapshot)) {
        handle_idle_state(current_snapshot);
        return;
    }

    if (current_snapshot.psel && !current_snapshot.psel_has_x) {
        m_statistics.record_bus_active_pclk_edge();
    }
    if (m_current_transaction.active && m_current_transaction.is_write) {
        m_current_transaction.pwdata_val = current_snapshot.pwdata;
        m_current_transaction.pwdata_val_has_x = current_snapshot.pwdata_has_x;
    }
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

void ApbAnalyzer::process_transaction_completion(const SignalState& snapshot_at_completion) {
    if (!m_current_transaction.active)
        return;

    // 1. 記錄這筆交易屬於哪個 Completer
    m_statistics.record_accessed_completer(m_current_transaction.target_completer);

    // 2. 檢查 Out-of-Range 錯誤
    check_for_out_of_range(snapshot_at_completion);

    // 3. 計算交易持續時間並記錄基本統計
    uint64_t duration_edges = m_current_pclk_edge_count - m_current_transaction.start_pclk_edge_count + 1;
    if (duration_edges < 2)
        duration_edges = 2;  // APB 最小交易週期

    if (m_current_transaction.is_write) {
        m_statistics.record_write_transaction(m_current_transaction.had_wait_state, duration_edges);
    } else {
        m_statistics.record_read_transaction(m_current_transaction.had_wait_state, duration_edges);
    }

    // 4. 與影子記憶體互動，並為短路分析收集樣本
    // 只處理沒有 'x' 且非 OOR 的有效交易
    if (!m_current_transaction.is_out_of_range) {
        if (m_current_transaction.is_write) {
            // 如果是寫交易，更新影子記憶體並收集樣本
            if (!m_current_transaction.paddr_val_has_x && !snapshot_at_completion.pwdata_has_x) {
                m_statistics.update_shadow_memory(
                    m_current_transaction.target_completer,
                    m_current_transaction.paddr,
                    snapshot_at_completion.pwdata,
                    snapshot_at_completion.timestamp_ps);
                m_statistics.add_pwdata_sample(m_current_transaction.target_completer, snapshot_at_completion.pwdata);
            }
        } else {  // 讀交易
            // 如果是讀交易，與影子記憶體進行比對
            if (!m_current_transaction.paddr_val_has_x && !snapshot_at_completion.prdata_has_x) {
                m_statistics.check_prdata_against_shadow_memory(
                    m_current_transaction.target_completer,
                    m_current_transaction.paddr,
                    snapshot_at_completion.prdata,
                    snapshot_at_completion.timestamp_ps);
            }
        }
        // 無論讀寫，都為 PADDR 收集樣本
        if (!m_current_transaction.paddr_val_has_x) {
            m_statistics.add_paddr_sample(m_current_transaction.target_completer, m_current_transaction.paddr);
        }
    }

    // 5. 重置交易狀態
    m_current_transaction.reset();
    m_current_apb_fsm_state = ApbFsmState::IDLE;
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
        m_current_transaction.is_out_of_range = false;
        if (!snapshot.paddr_has_x) {
            m_current_transaction.target_completer = get_completer_id_from_paddr(snapshot.paddr);
        } else {
            m_current_transaction.target_completer = CompleterID::UNKNOWN_COMPLETER;
        }
    }
}

void ApbAnalyzer::handle_setup_state(const SignalState& snapshot) {
    if (!m_current_transaction.active) {
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }
    if (!snapshot.psel || snapshot.psel_has_x) {  // PSEL 掉低或變 X
        if (m_current_transaction.active) {
            m_current_transaction.reset();
        }
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }

    if (snapshot.penable && !snapshot.penable_has_x) {
        m_current_apb_fsm_state = ApbFsmState::ACCESS;

        if (!snapshot.pready && !snapshot.pready_has_x) {
            m_current_transaction.had_wait_state = true;
        } else if (snapshot.pready && !snapshot.pready_has_x) {
            process_transaction_completion(snapshot);  // 呼叫集中的處理函式
        } else if (snapshot.pready_has_x) {
            m_current_transaction.had_wait_state = true;
        }
    }
}

void ApbAnalyzer::handle_access_state(const SignalState& snapshot) {
    if (!m_current_transaction.active) {
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }
    if (!snapshot.psel || snapshot.psel_has_x || !snapshot.penable || snapshot.penable_has_x) {
        if (m_current_transaction.active) {
            m_current_transaction.reset();
        }
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }

    if (snapshot.pready && !snapshot.pready_has_x) {
        process_transaction_completion(snapshot);  // 呼叫集中的處理函式
    } else if (!snapshot.pready && !snapshot.pready_has_x) {
        m_current_transaction.had_wait_state = true;
    } else if (snapshot.pready_has_x) {
        m_current_transaction.had_wait_state = true;
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
    m_statistics.analyze_bus_shorts();
}

bool ApbAnalyzer::check_for_timeout(const SignalState& current_snapshot) {
    if (!m_current_transaction.active) {
        return false;
    }
    uint64_t elapsed_cycles = m_current_pclk_edge_count - m_current_transaction.start_pclk_edge_count + 1;
    if (elapsed_cycles >= TIMEOUT_THRESHOLD_CYCLES) {
        TransactionTimeoutDetail detail{
            m_current_transaction.transaction_start_time_ps,
            current_snapshot.timestamp_ps,
            m_current_transaction.paddr,
            elapsed_cycles};

        m_current_transaction.reset();
        m_current_apb_fsm_state = ApbFsmState::IDLE;
    }
    return false;
}  // namespace APBSystem
}  // namespace APBSystem