#include "apb_analyzer.hpp"
#include <iomanip>
#include <iostream>

namespace APBSystem {

ApbAnalyzer::ApbAnalyzer(Statistics& statistics /*, std::ostream& debug_stream*/)
    : m_statistics(statistics) /*, m_debug_stream(debug_stream)*/, m_current_apb_fsm_state(ApbFsmState::IDLE), m_current_pclk_edge_count(0), m_system_out_of_reset(false), m_first_valid_pclk_edge_for_stats(0), m_transaction_cycle_counter(0), m_completed_transaction_count(0) {
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
    m_current_transaction.transaction_start_time_ps = snapshot.timestamp;
    m_current_transaction.is_write = snapshot.pwrite && !snapshot.pwrite_has_x;
    m_current_transaction.paddr = snapshot.paddr;
    m_current_transaction.paddr_val_has_x = snapshot.paddr_has_x;
    m_current_transaction.pwdata_val = snapshot.pwdata;
    m_current_transaction.pwdata_val_has_x = snapshot.pwdata_has_x;
    m_transaction_cycle_counter = 1;
    m_current_transaction.target_completer = snapshot.paddr_has_x ? CompleterID::UNKNOWN_COMPLETER : get_completer_id_from_paddr(snapshot.paddr);
    if (m_current_transaction.is_write) {
        m_pending_writes[m_current_transaction.paddr] = {snapshot.timestamp, m_current_pclk_edge_count};
    } else {
        auto it = m_pending_writes.find(m_current_transaction.paddr);
        if (it != m_pending_writes.end()) {
            m_preliminary_overlap_errors.push_back({{snapshot.timestamp, m_current_transaction.paddr},
                                                    it->second.start_time_ps,
                                                    it->first});
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
    if (!m_current_transaction.active || m_transaction_cycle_counter <= 1000)
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
    m_completed_transaction_count++;
    if (m_current_transaction.is_write)
        m_pending_writes.erase(m_current_transaction.paddr);
    m_statistics.record_accessed_completer(m_current_transaction.target_completer);
    if (!m_current_transaction.paddr_val_has_x) {
        m_statistics.record_paddr_for_corruption_analysis(m_current_transaction.target_completer, m_current_transaction.paddr);
    }
    if (m_current_transaction.is_write && !snapshot.pwdata_has_x) {
        m_statistics.record_pwdata_for_corruption_analysis(m_current_transaction.target_completer, snapshot.pwdata);
    }
    preliminary_check_for_out_of_range(snapshot);
    uint64_t duration = m_current_pclk_edge_count - m_current_transaction.start_pclk_edge_count + 1;
    if (m_current_transaction.is_write)
        m_statistics.record_write_transaction(m_current_transaction.had_wait_state, duration);
    else
        m_statistics.record_read_transaction(m_current_transaction.had_wait_state, duration);
    if (!m_current_transaction.is_out_of_range) {
        if (m_current_transaction.is_write && !m_current_transaction.paddr_val_has_x && !snapshot.pwdata_has_x) {
            m_statistics.update_shadow_memory(m_current_transaction.target_completer, m_current_transaction.paddr, snapshot.pwdata, snapshot.timestamp);
        } else if (!m_current_transaction.is_write && !m_current_transaction.paddr_val_has_x && !snapshot.prdata_has_x) {
            m_statistics.check_for_data_mirroring(m_current_transaction.target_completer, m_current_transaction.paddr, snapshot.prdata, snapshot.timestamp);
        }
    }
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
    filter_and_commit_errors();
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
void ApbAnalyzer::filter_and_commit_errors() {
    for (const auto& oor_error : m_preliminary_oor_errors) {
        CompleterID cid = get_completer_id_from_paddr(oor_error.paddr);
        if (!m_statistics.is_completer_corrupted(cid)) {
            m_statistics.record_out_of_range_access(oor_error);
        }
    }

    for (const auto& overlap_error : m_preliminary_overlap_errors) {
        if (!m_statistics.is_transaction_timeout(overlap_error.write_start_time, overlap_error.write_paddr)) {
            m_statistics.record_read_write_overlap_error(overlap_error.detail);
        }
    }
}
void ApbAnalyzer::preliminary_check_for_out_of_range(const SignalState& snapshot) {
    if (!m_current_transaction.active || m_current_transaction.paddr_val_has_x)
        return;
    if (m_current_transaction.target_completer == CompleterID::UNKNOWN_COMPLETER) {
        m_preliminary_oor_errors.push_back({snapshot.timestamp, m_current_transaction.paddr});
    }
}

}  // namespace APBSystem
