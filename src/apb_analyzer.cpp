// apb_analyzer.cpp
#include "apb_analyzer.hpp"
#include <iostream>

namespace APBSystem {

// 建構函式和未修改的函式保持不變
ApbAnalyzer::ApbAnalyzer(Statistics& statistics)
    : m_statistics(statistics),
      m_current_apb_fsm_state(ApbFsmState::IDLE),
      m_current_pclk_edge_count(0),
      m_system_out_of_reset(false),
      m_first_valid_pclk_edge_for_stats(0),
      m_transaction_cycle_counter(0) {
    m_current_transaction.reset();
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

    if (m_current_transaction.active) {
        m_transaction_cycle_counter++;
    }

    if (check_for_timeout(current_snapshot)) {
        return;
    }
    if (current_snapshot.psel && !current_snapshot.psel_has_x) {
        m_statistics.record_bus_active_pclk_edge();
    }

    ApbFsmState state_before = m_current_apb_fsm_state;

    switch (state_before) {
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

    if ((state_before == ApbFsmState::SETUP || state_before == ApbFsmState::ACCESS) &&
        m_current_apb_fsm_state == ApbFsmState::IDLE) {
        handle_idle_state(current_snapshot);
    }

    if (state_before == ApbFsmState::SETUP && m_current_apb_fsm_state == ApbFsmState::ACCESS) {
        handle_access_state(current_snapshot);
    }
}

void ApbAnalyzer::handle_idle_state(const SignalState& snapshot) {
    if (!snapshot.psel || snapshot.psel_has_x || snapshot.penable) {
        return;
    }

    bool is_write = snapshot.pwrite && !snapshot.pwrite_has_x;
    uint32_t paddr = snapshot.paddr;

    if (!is_write) {
        auto it = m_pending_writes.find(paddr);
        if (it != m_pending_writes.end()) {
            uint64_t pend_cycles = m_current_pclk_edge_count - it->second.start_pclk_edge_count;
            if (pend_cycles <= 100) {
                m_statistics.record_read_write_overlap_error({snapshot.timestamp_ps, paddr});
            }
        }
    } else {
        if (m_pending_writes.find(paddr) == m_pending_writes.end()) {
            m_pending_writes[paddr] = {snapshot.timestamp_ps, m_current_pclk_edge_count};
        }
    }

    m_current_apb_fsm_state = ApbFsmState::SETUP;
    m_current_transaction.active = true;
    m_current_transaction.start_pclk_edge_count = m_current_pclk_edge_count;
    m_transaction_cycle_counter = 1;
    m_current_transaction.transaction_start_time_ps = snapshot.timestamp_ps;
    m_current_transaction.is_write = is_write;
    m_current_transaction.paddr = paddr;
    m_current_transaction.paddr_val_has_x = snapshot.paddr_has_x;
    if (m_current_transaction.is_write) {
        m_current_transaction.pwdata_val = snapshot.pwdata;
        m_current_transaction.pwdata_val_has_x = snapshot.pwdata_has_x;
    }
    m_current_transaction.had_wait_state = false;
    m_current_transaction.is_out_of_range = false;
    if (!snapshot.paddr_has_x) {
        m_current_transaction.target_completer = get_completer_id_from_paddr(snapshot.paddr);
    } else {
        m_current_transaction.target_completer = CompleterID::UNKNOWN_COMPLETER;
    }
}

void ApbAnalyzer::handle_setup_state(const SignalState& snapshot) {
    if (!m_current_transaction.active) {
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }
    if (!snapshot.psel || snapshot.psel_has_x) {
        m_current_transaction.reset();
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }
    if (snapshot.penable && !snapshot.penable_has_x) {
        m_current_apb_fsm_state = ApbFsmState::ACCESS;
    }
}

void ApbAnalyzer::handle_access_state(const SignalState& snapshot) {
    if (!m_current_transaction.active) {
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }

    // 條件 1: 交易正常完成
    if (snapshot.pready && !snapshot.pready_has_x) {
        process_transaction_completion(snapshot);
        return;
    }

    // 條件 2: 交易被 PSEL 掉低中止
    if (!snapshot.psel || snapshot.psel_has_x) {
        m_current_transaction.reset();
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return;
    }

    // 條件 3: 交易被新的 Setup Phase (PENABLE 掉低) 中斷
    // 這正是 ReadWriteOverlap.vcd 中的情況
    if (!snapshot.penable || snapshot.penable_has_x) {
        m_current_transaction.reset();                // 舊交易中止
        m_current_apb_fsm_state = ApbFsmState::IDLE;  // 返回 IDLE 以便主迴圈重新處理
        return;
    }

    // 若以上情況都未發生，則為正常的 wait state
    m_current_transaction.had_wait_state = true;
}

bool ApbAnalyzer::check_for_timeout(const SignalState& current_snapshot) {
    if (!m_current_transaction.active) {
        return false;
    }
    if (m_transaction_cycle_counter > 100) {
        TransactionTimeoutDetail detail{
            m_current_transaction.transaction_start_time_ps,
            current_snapshot.timestamp_ps,
            m_current_transaction.paddr,
            m_transaction_cycle_counter};
        m_statistics.record_timeout_error(detail);

        if (m_current_transaction.is_write) {
            m_pending_writes.erase(m_current_transaction.paddr);
        }

        m_current_transaction.reset();
        m_current_apb_fsm_state = ApbFsmState::IDLE;
        return true;
    }
    return false;
}

void ApbAnalyzer::process_transaction_completion(const SignalState& snapshot_at_completion) {
    if (!m_current_transaction.active)
        return;
    if (m_current_transaction.is_write) {
        m_pending_writes.erase(m_current_transaction.paddr);
    }
    m_statistics.record_accessed_completer(m_current_transaction.target_completer);
    check_for_out_of_range(snapshot_at_completion);
    uint64_t duration_edges = m_current_pclk_edge_count - m_current_transaction.start_pclk_edge_count + 1;
    if (duration_edges < 2)
        duration_edges = 2;
    if (m_current_transaction.is_write) {
        m_statistics.record_write_transaction(m_current_transaction.had_wait_state, duration_edges);
    } else {
        m_statistics.record_read_transaction(m_current_transaction.had_wait_state, duration_edges);
    }
    if (!m_current_transaction.is_out_of_range) {
        if (m_current_transaction.is_write) {
            if (!m_current_transaction.paddr_val_has_x && !snapshot_at_completion.pwdata_has_x) {
                m_statistics.update_shadow_memory(
                    m_current_transaction.target_completer,
                    m_current_transaction.paddr,
                    snapshot_at_completion.pwdata,
                    snapshot_at_completion.timestamp_ps);
            }
        } else {
            if (!m_current_transaction.paddr_val_has_x && !snapshot_at_completion.prdata_has_x) {
                m_statistics.check_prdata_against_shadow_memory(
                    m_current_transaction.target_completer,
                    m_current_transaction.paddr,
                    snapshot_at_completion.prdata,
                    snapshot_at_completion.timestamp_ps);
            }
        }
    }
    m_current_transaction.reset();
    m_current_apb_fsm_state = ApbFsmState::IDLE;
}

void ApbAnalyzer::finalize_analysis(uint64_t final_vcd_timestamp_ps) {
    if (m_current_transaction.active) {
        if (m_current_transaction.is_write) {
            m_pending_writes.erase(m_current_transaction.paddr);
        }
        m_current_transaction.reset();
    }
    m_current_apb_fsm_state = ApbFsmState::IDLE;
    m_statistics.set_first_valid_pclk_edge_for_stats(m_first_valid_pclk_edge_for_stats);
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
            if (current_paddr < UART_BASE_ADDR || current_paddr > UART_END_ADDR)
                is_oor = true;
            break;
        case CompleterID::GPIO:
            if (current_paddr < GPIO_BASE_ADDR || current_paddr > GPIO_END_ADDR)
                is_oor = true;
            break;
        case CompleterID::SPI_MASTER:
            if (current_paddr < SPI_MASTER_BASE_ADDR || current_paddr > SPI_MASTER_END_ADDR)
                is_oor = true;
            break;
        case CompleterID::UNKNOWN_COMPLETER:
            is_oor = true;
            break;
        case CompleterID::NONE:
            is_oor = true;
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
}  // namespace APBSystem
