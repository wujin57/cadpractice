#include "apb_analyzer.hpp"
#include <iostream>
#include <sstream>

namespace APBSystem {
ApbAnalyzer::ApbAnalyzer(Statistics& stats_collector,
                         ErrorLogger& error_lgr,
                         const SignalManager& sig_mgr_ref)
    : stats_(stats_collector),
      error_logger_(error_lgr),
      signal_manager_(sig_mgr_ref),
      current_vcd_time_(0),
      current_pclk_cycle_count_since_reset_(0),
      fsm_state_(ApbFsmState::IDLE),
      in_transaction_(false),
      current_transaction_start_vcd_time_(0),
      current_transaction_start_pclk_cycle_(0),
      current_transaction_paddr_val_(0),
      current_transaction_is_read_(false),
      current_transaction_pwdata_val_(0),
      current_transaction_target_completer_(CompleterLogicalID::UNKNOWN),
      access_phase_pclk_count_(0) {}

void ApbAnalyzer::reset_analyzer_for_new_cycle_or_reset() {
    fsm_state_ = ApbFsmState::IDLE;
    in_transaction_ = false;
    access_phase_pclk_count_ = 0;
    current_transaction_target_completer_ = CompleterLogicalID::UNKNOWN;
    current_transaction_start_vcd_time_ = 0;
    current_transaction_start_pclk_cycle_ = 0;
    current_transaction_paddr_val_ = 0;
    current_transaction_is_read_ = false;
    current_transaction_pwdata_val_ = 0;
}

CompleterLogicalID ApbAnalyzer::identify_target_completer(const SignalValues& signals) const {
    if (signals.PSEL_UART)
        return CompleterLogicalID::UART;
    if (signals.PSEL_GPIO)
        return CompleterLogicalID::GPIO;
    if (signals.PSEL_SPI_MASTER)
        return CompleterLogicalID::SPI_MASTER;
    return CompleterLogicalID::UNKNOWN;
}

void ApbAnalyzer::process_pclk_event(long long current_vcd_time, const SignalValues& current_signals) {
    current_vcd_time_ = current_vcd_time;

    if (!current_signals.PRESETn) {
        reset_analyzer_for_new_cycle_or_reset();
        current_pclk_cycle_count_since_reset_ = 0;

        return;
    }

    current_pclk_cycle_count_since_reset_++;
    stats_.record_pclk_cycle();

    bool is_bus_active_now = current_signals.is_any_psel_active();
    if (is_bus_active_now) {
        stats_.record_active_bus_cycle();
    } else {
        stats_.record_idle_cycle();
    }

    switch (fsm_state_) {
        case ApbFsmState::IDLE:
            handle_idle_state(current_signals);
            break;
        case ApbFsmState::SETUP:
            handle_setup_state(current_signals);
            break;
        case ApbFsmState::ACCESS:
            handle_access_state(current_signals, current_vcd_time);
            break;
    }
}

void ApbAnalyzer::handle_idle_state(const SignalValues& current_signals) {
    if (current_signals.is_any_psel_active() && !current_signals.PENABLE) {
        fsm_state_ = ApbFsmState::SETUP;

        current_transaction_start_vcd_time_ = current_vcd_time_;
        current_transaction_start_pclk_cycle_ = current_pclk_cycle_count_since_reset_;
        current_transaction_paddr_val_ = current_signals.PADDR;
        current_transaction_is_read_ = !current_signals.PWRITE;
        if (!current_transaction_is_read_) {
            current_transaction_pwdata_val_ = current_signals.PWDATA;
        } else {
            current_transaction_pwdata_val_ = 0;
        }
        current_transaction_target_completer_ = identify_target_completer(current_signals);
        access_phase_pclk_count_ = 0;

        check_setup_phase_errors(current_signals);
    }
}

void ApbAnalyzer::handle_setup_state(const SignalValues& current_signals) {
    check_setup_phase_errors(current_signals);
    if (current_signals.PENABLE) {
        fsm_state_ = ApbFsmState::ACCESS;
        access_phase_pclk_count_ = 1;
    }
}

void ApbAnalyzer::handle_access_state(const SignalValues& current_signals, long long current_vcd_time) {
    if (!current_signals.is_any_psel_active() || !current_signals.PENABLE) {
        fsm_state_ = ApbFsmState::IDLE;
        return;
    }

    check_access_phase_errors(current_signals);
    if (current_signals.PREADY) {
        APBSystem::TransactionData tx_data;

        tx_data.start_time_ = current_transaction_start_vcd_time_;
        tx_data.end_time_ = current_vcd_time;
        tx_data.paddr_ = current_transaction_paddr_val_;
        tx_data.is_write_ = current_transaction_is_read_;

        if (tx_data.is_write_) {
            tx_data.pwdata_ = current_transaction_pwdata_val_;
        }

        tx_data.completer_id_ = current_transaction_target_completer_;
        tx_data.duration_pclk_cycles_ = 1 + access_phase_pclk_count_;
        tx_data.has_wait_states_ = (access_phase_pclk_count_ > 1);

        stats_.record_transaction_completion(tx_data);
        fsm_state_ = ApbFsmState::IDLE;
    } else {  // PREADY is not high yet
        access_phase_pclk_count_++;
        if (access_phase_pclk_count_ > MAX_TIMEOUT_PCLK_CYCLES) {
            // error_logger
        }
    }
}

void ApbAnalyzer::check_setup_phase_errors(const SignalValues& current_signals) {
}

void ApbAnalyzer::check_access_phase_errors(const SignalValues& current_signals) {
}

void ApbAnalyzer::finalize_analysis_at_vcd_end() {
}
}  // namespace APBSystem