#include "apb_analyzer.hpp"
#include <iomanip>
#include <iostream>

namespace APBSystem {

// ... (fsm_state_to_string, 建構函式, analyze_on_pclk_rising_edge, handle_idle_state, handle_setup_state, handle_access_state, check_for_timeout, process_transaction_completion, finalize_analysis, get_completer_id_from_paddr, check_for_out_of_range 皆無變動)
ApbAnalyzer::ApbAnalyzer(Statistics& statistics, std::ostream& debug_stream)
    : m_statistics(statistics), m_debug_stream(debug_stream), m_current_apb_fsm_state(ApbFsmState::IDLE), m_current_pclk_edge_count(0), m_system_out_of_reset(false), m_first_valid_pclk_edge_for_stats(0), m_transaction_cycle_counter(0) {
    m_current_transaction.reset();
}
void ApbAnalyzer::analyze_on_pclk_rising_edge(const SignalState& snapshot, uint64_t pclk_edge_count) {
    m_current_pclk_edge_count = pclk_edge_count;
    if (!m_system_out_of_reset) {
        if (snapshot.presetn) {
            m_system_out_of_reset = true;
            m_first_valid_pclk_edge_for_stats = pclk_edge_count;
        } else
            return;
    }
    if (m_current_transaction.active)
        m_transaction_cycle_counter++;
    if (check_for_timeout(snapshot))
        return;
    if (snapshot.psel && !snapshot.psel_has_x)
        m_statistics.record_bus_active_pclk_edge();
    ApbFsmState state_before = m_current_apb_fsm_state;
    if (state_before == ApbFsmState::IDLE) {
        handle_idle_state(snapshot);
    } else if (state_before == ApbFsmState::SETUP) {
        handle_setup_state(snapshot);
    }
    if (m_current_apb_fsm_state == ApbFsmState::ACCESS) {
        handle_access_state(snapshot);
    }
}
void ApbAnalyzer::handle_idle_state(const SignalState& snapshot) {
    if (!snapshot.psel || snapshot.psel_has_x || snapshot.penable)
        return;
    m_current_apb_fsm_state = ApbFsmState::SETUP;
    m_current_transaction.active = true;
    m_current_transaction.start_pclk_edge_count = m_current_pclk_edge_count;
    m_current_transaction.transaction_start_time_ps = snapshot.timestamp_ps;
    m_current_transaction.is_write = snapshot.pwrite && !snapshot.pwrite_has_x;
    m_current_transaction.paddr = snapshot.paddr;
    m_current_transaction.paddr_val_has_x = snapshot.paddr_has_x;
    m_current_transaction.pwdata_val = snapshot.pwdata;
    m_current_transaction.pwdata_val_has_x = snapshot.pwdata_has_x;
    m_transaction_cycle_counter = 1;
    m_current_transaction.target_completer = snapshot.paddr_has_x ? CompleterID::UNKNOWN_COMPLETER : get_completer_id_from_paddr(snapshot.paddr);
    if (m_current_transaction.is_write) {
        m_pending_writes[m_current_transaction.paddr] = {snapshot.timestamp_ps, m_current_pclk_edge_count};
    } else {
        if (m_pending_writes.count(m_current_transaction.paddr)) {
            m_statistics.record_read_write_overlap_error({snapshot.timestamp_ps, m_current_transaction.paddr});
        }
    }
}
void ApbAnalyzer::handle_setup_state(const SignalState& snapshot) {
    if (!m_current_transaction.active) {
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }
    if (!snapshot.psel || snapshot.psel_has_x) {
        if (m_current_transaction.is_write)
            m_pending_writes.erase(m_current_transaction.paddr);
        m_current_transaction.reset();
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }
    if (snapshot.penable && !snapshot.penable_has_x) {
        m_current_apb_fsm_state = ApbFsmState::ACCESS;
        m_current_transaction.pwdata_val = snapshot.pwdata;
        m_current_transaction.pwdata_val_has_x = snapshot.pwdata_has_x;
    }
}
void ApbAnalyzer::handle_access_state(const SignalState& snapshot) {
    if (!m_current_transaction.active) {
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }
    if (snapshot.pready && !snapshot.pready_has_x) {
        process_transaction_completion(snapshot);
        return;
    }
    if (!snapshot.psel || snapshot.psel_has_x || (!snapshot.penable && !snapshot.penable_has_x)) {
        if (m_current_transaction.is_write)
            m_pending_writes.erase(m_current_transaction.paddr);
        m_current_transaction.reset();
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }
    m_current_transaction.had_wait_state = true;
}
bool ApbAnalyzer::check_for_timeout(const SignalState& snapshot) {
    if (!m_current_transaction.active || m_transaction_cycle_counter <= 100)
        return false;
    m_statistics.record_timeout_error({m_current_transaction.transaction_start_time_ps, m_current_transaction.paddr});
    if (m_current_transaction.is_write)
        m_pending_writes.erase(m_current_transaction.paddr);
    m_current_transaction.reset();
    m_current_apb_fsm_state = ApbFsmState::IDLE;
    return true;
}
void ApbAnalyzer::process_transaction_completion(const SignalState& snapshot) {
    if (!m_current_transaction.active)
        return;
    CompleterID cid = m_current_transaction.target_completer;
    m_statistics.ensure_activity(cid);
    if (m_current_transaction.is_write)
        m_pending_writes.erase(m_current_transaction.paddr);
    if (!m_current_transaction.paddr_val_has_x) {
        m_statistics.record_paddr_sample(m_current_transaction.target_completer, m_current_transaction.paddr);
    }
    if (m_current_transaction.is_write && !snapshot.pwdata_has_x) {
        m_statistics.record_pwdata_sample(m_current_transaction.target_completer, snapshot.pwdata);
    }
    check_for_out_of_range(snapshot);
    uint64_t duration = m_current_pclk_edge_count - m_current_transaction.start_pclk_edge_count + 1;
    if (m_current_transaction.is_write)
        m_statistics.record_write_transaction(m_current_transaction.had_wait_state, duration);
    else
        m_statistics.record_read_transaction(m_current_transaction.had_wait_state, duration);
    if (!m_current_transaction.is_out_of_range) {
        if (m_current_transaction.is_write && !m_current_transaction.paddr_val_has_x && !snapshot.pwdata_has_x) {
            m_statistics.update_shadow_memory(m_current_transaction.target_completer, m_current_transaction.paddr, snapshot.pwdata, snapshot.timestamp_ps);
        } else if (!m_current_transaction.is_write && !m_current_transaction.paddr_val_has_x && !snapshot.prdata_has_x) {
            m_statistics.check_for_data_mirroring(m_current_transaction.target_completer, m_current_transaction.paddr, snapshot.prdata, snapshot.timestamp_ps);
        }
    }
    m_current_transaction.pwdata_val = snapshot.pwdata;
    m_current_transaction.pwdata_val_has_x = snapshot.pwdata_has_x;
    m_completed_transactions.push_back(m_current_transaction);
    m_current_transaction.reset();
    m_current_apb_fsm_state = ApbFsmState::IDLE;
}
void ApbAnalyzer::finalize_analysis(uint64_t final_ts) {
    if (m_current_transaction.active) {
        if (m_current_transaction.is_write)
            m_pending_writes.erase(m_current_transaction.paddr);
        m_current_transaction.reset();
    }
    m_statistics.set_first_valid_pclk_edge_for_stats(m_first_valid_pclk_edge_for_stats);
    m_statistics.finalize_bit_activity();
    detect_all_corruption_errors();
}
CompleterID ApbAnalyzer::get_completer_id_from_paddr(uint32_t paddr) const {
    if (paddr >= UART_BASE_ADDR && paddr <= UART_END_ADDR)
        return CompleterID::UART;
    if (paddr >= GPIO_BASE_ADDR && paddr <= GPIO_END_ADDR)
        return CompleterID::GPIO;
    if (paddr >= SPI_MASTER_BASE_ADDR && paddr <= SPI_MASTER_END_ADDR)
        return CompleterID::SPI_MASTER;
    return CompleterID::UNKNOWN_COMPLETER;
}
void ApbAnalyzer::check_for_out_of_range(const SignalState& snapshot) {
    if (!m_current_transaction.active || m_current_transaction.paddr_val_has_x) {
        m_current_transaction.is_out_of_range = true;
        return;
    }
    if (m_current_transaction.target_completer == CompleterID::UNKNOWN_COMPLETER) {
        m_current_transaction.is_out_of_range = true;
        m_statistics.record_out_of_range_access({snapshot.timestamp_ps, m_current_transaction.paddr});
    } else {
        m_current_transaction.is_out_of_range = false;
    }
}

// MODIFIED: 重構此函式以提高穩健性
void ApbAnalyzer::detect_all_corruption_errors() {
    const auto& activity_map = m_statistics.get_completer_bit_activity_map();

    for (const auto& transaction : m_completed_transactions) {
        if (transaction.paddr_val_has_x)
            continue;

        CompleterID cid = transaction.target_completer;
        if (cid == CompleterID::NONE || cid == CompleterID::UNKNOWN_COMPLETER)
            continue;

        auto it = activity_map.find(cid);
        if (it == activity_map.end())
            continue;

        const auto& activity = it->second;
        bool error_found_for_this_transaction = false;

        // 偵測 Address Corruption
        if (!activity.paddr_bit_details.empty()) {  // 安全性檢查
            for (size_t i = 0; i < activity.paddr_bit_details.size(); ++i) {
                const auto& bi = activity.paddr_bit_details[i];
                if (bi.status == BitConnectionStatus::SHORTED && bi.shorted_with_bit_index > static_cast<int>(i)) {
                    m_statistics.record_address_corruption({transaction.transaction_start_time_ps, transaction.paddr, static_cast<int>(i), bi.shorted_with_bit_index});
                    error_found_for_this_transaction = true;
                    break;  // 找到一對短路就足以標記此交易，跳出內層迴圈
                }
            }
        }

        if (error_found_for_this_transaction) {
            continue;  // 既然已找到位址錯誤，就繼續處理下一筆交易
        }

        // 偵測 Data Corruption (僅針對寫入交易)
        if (transaction.is_write && !transaction.pwdata_val_has_x) {
            if (!activity.pwdata_bit_details.empty()) {  // 安全性檢查
                for (size_t i = 0; i < activity.pwdata_bit_details.size(); ++i) {
                    const auto& bi = activity.pwdata_bit_details[i];
                    if (bi.status == BitConnectionStatus::SHORTED && bi.shorted_with_bit_index > static_cast<int>(i)) {
                        m_statistics.record_data_corruption({transaction.transaction_start_time_ps, transaction.paddr, transaction.pwdata_val, static_cast<int>(i), bi.shorted_with_bit_index});
                        break;  // 找到一對短路就足以標記此交易，跳出內層迴圈
                    }
                }
            }
        }
    }
}

}  // namespace APBSystem
