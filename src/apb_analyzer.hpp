// apb_analyzer.hpp
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "apb_types.hpp"
#include "error_logger.hpp"
#include "signal_manager.hpp"
#include "statistics.hpp"

namespace APBSystem {

class ApbAnalyzer {
   public:
    ApbAnalyzer(Statistics& stats_collector,
                ErrorLogger& error_lgr,
                const SignalManager& sig_mgr_ref);  // Pass SignalManager by const reference
    // Main processing function, called on each PCLK posedge after PRESETn de-assertion
    void process_pclk_event(long long current_vcd_time, const SignalValues& current_signals);

    // Call this when PRESETn is active or to reset FSM
    void reset_analyzer_for_new_cycle_or_reset();

    // Call at the very end of VCD processing to handle any unterminated transaction
    void finalize_analysis_at_vcd_end();

   private:
    // FSM state handlers
    void handle_idle_state(const SignalValues& current_signals);
    void handle_setup_state(const SignalValues& current_signals);
    void handle_access_state(const SignalValues& current_signals, long long current_vcd_time);

    // Helper to identify target completer based on active PSEL signals
    CompleterLogicalID identify_target_completer(const SignalValues& signals) const;

    // Basic error check example stubs
    void check_setup_phase_errors(const SignalValues& current_signals);
    void check_access_phase_errors(const SignalValues& current_signals);

    // References to other modules
    Statistics& stats_;
    ErrorLogger& error_logger_;
    const SignalManager& signal_manager_;

    // Time and cycle tracking (relative to analysis start after reset)
    long long current_vcd_time_;
    long long current_pclk_cycle_count_since_reset_;

    // FSM state
    ApbFsmState fsm_state_;
    bool in_transaction_;  // True if a transaction (setup or access) is active

    // Current transaction tracking variables
    long long current_transaction_start_vcd_time_;
    long long current_transaction_start_pclk_cycle_;
    uint32_t current_transaction_paddr_val_;
    bool current_transaction_is_read_;
    uint32_t current_transaction_pwdata_val_;
    CompleterLogicalID current_transaction_target_completer_;

    int access_phase_pclk_count_;  // Cycles in access phase (PENABLE high until PREADY high)
                                   // Starts at 1 for the first cycle PENABLE is high.

    // For PREADY Timeout (E06) tracking
    long long pclk_cycles_since_penable_high_;  // Alternative to access_phase_pclk_count_ for timeout specific count
                                                // Or simply use access_phase_pclk_count_ for timeout.
                                                // For simplicity, we will use access_phase_pclk_count_
};

}  // namespace APBSystem