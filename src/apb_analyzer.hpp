// apb_analyzer.hpp
#pragma once

#include "apb_types.hpp"
#include "error_logger.hpp"
#include "signal_manager.hpp"  // To access SignalManager for signal properties if needed
#include "statistics.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace APBSystem {

class ApbAnalyzer {
   public:
    ApbAnalyzer(Statistics& stats_collector,
                ErrorLogger& error_lgr,
                const SignalManager& sig_mgr_ref);  // Pass SignalManager by const reference

    void process_vcd_timestamp(int time);
    // Called on PCLK rising edge, triggers FSM and main logic
    void on_pclk_rising_edge();
    // Called after all VCD definitions are parsed (e.g., from $enddefinitions)
    void finalize_signal_definitions();
    // Called after all VCD value changes are processed (end of VCD parsing)
    void finalize_analysis_and_fault_identification();

    // Getters for PADDR/PWDATA fault information for the report
    const IdentifiedFloatingPair& get_paddr_fault_info(int completer_id) const;
    const IdentifiedFloatingPair& get_pwdata_fault_info(int completer_id) const;
    int get_paddr_bus_width() const { return paddr_actual_width_; }
    int get_pwdata_bus_width() const { return pwdata_actual_width_; }

   private:
    // --- Member Variables ---
    SignalState current_signal_state_;  // Current state of APB signals
    SignalState prev_signal_state_;     // State of APB signals in the previous PCLK cycle

    int current_vcd_time_ = 0;
    long long current_pclk_cycle_count_ = 0;

    ApbFsmState fsm_state_ = ApbFsmState::IDLE;

    // Transaction tracking
    bool in_transaction_ = false;
    uint32_t latched_paddr_ = 0;
    bool latched_pwrite_ = false;
    int transaction_start_pclk_cycle_ = 0;
    int access_phase_pclk_count_ = 0;  // For timeout
    int current_transaction_target_completer_id_ = -1;
    uint32_t latched_pwdata_for_instability_check_ = 0;
    bool paddr_stable_in_access_ = true;
    bool pwdata_stable_in_access_ = true;

    // References to other modules
    Statistics& stats_;
    ErrorLogger& error_logger_;
    const SignalManager& signal_manager_;  // Store the reference

    // Fault discovery structures (per completer)
    BitPairComparisonStats paddr_bit_stats_[MAX_COMPLETERS][MAX_SIGNAL_BITS][MAX_SIGNAL_BITS];
    BitPairComparisonStats pwdata_bit_stats_[MAX_COMPLETERS][MAX_SIGNAL_BITS][MAX_SIGNAL_BITS];
    IdentifiedFloatingPair identified_paddr_faults_[MAX_COMPLETERS];
    IdentifiedFloatingPair identified_pwdata_faults_[MAX_COMPLETERS];

    int paddr_actual_width_ = MAX_SIGNAL_BITS;   // Will be updated from SignalManager
    int pwdata_actual_width_ = MAX_SIGNAL_BITS;  // Will be updated from SignalManager

    // States for Read-Write Overlap
    struct ActiveWriteInfo {
        uint32_t paddr;
        int completer_id;
        // Add more if needed, like start PCLK cycle
    };
    std::optional<ActiveWriteInfo> active_write_transaction_;

    // --- Private Helper Methods ---
    void reset_analyzer_state();  // Resets FSM and transaction tracking
    void update_internal_signal_states(const SignalState& new_vcd_driven_state);

    // Completer ID determination (CRUCIAL - NEEDS YOUR IMPLEMENTATION)
    int determine_target_completer_id(uint32_t paddr) const;

    // APB FSM and transaction lifecycle
    void run_apb_fsm();
    void handle_idle_state();
    void handle_setup_state();
    void handle_access_state();
    void start_new_transaction();
    void complete_current_transaction(bool is_error_termination);

    // Error detection methods
    void check_paddr_pwdata_instability();
    void check_pslverr();
    void check_timeout();
    void check_protocol_errors_on_end();  // e.g. PSEL/PENABLE drops
    void detect_and_log_address_corruption(int completer_id, uint32_t expected_paddr, uint32_t received_paddr);
    void detect_and_log_data_corruption(int completer_id, uint32_t expected_pwdata, uint32_t received_pwdata);
    void detect_out_of_range_access(int target_completer_id, uint32_t paddr_val);
    // void detect_data_mirroring(); // Complex, can be added later
    void detect_read_write_overlap(uint32_t read_paddr, int read_completer_id);

    // Fault discovery logic (adapted from your transaction.cpp)
    void initialize_fault_discovery_structures_();
    void update_paddr_fault_discovery_stats(int completer_id, uint32_t paddr_val);
    void update_pwdata_fault_discovery_stats(int completer_id, uint32_t pwdata_val);
    void identify_fixed_faulty_pairs_per_completer(int completer_id);

    // Formatting error messages
    std::string format_addr_corr_msg(uint32_t expected, uint32_t received, int bit_fault1, int bit_fault2) const;
    std::string format_data_corr_msg(uint32_t expected, uint32_t received, int bit_fault1, int bit_fault2) const;
    std::string format_out_of_range_msg(uint32_t paddr_val, int completer_id) const;
    std::string format_rw_overlap_msg(uint32_t paddr_val) const;
    std::string format_timeout_msg(uint32_t paddr_val, int cycle_count) const;
    std::string format_pslverr_msg(uint32_t paddr_val) const;
    std::string format_instability_msg(const std::string& signal_name, uint32_t paddr_val) const;
    std::string format_protocol_error_msg(const std::string& reason, uint32_t paddr_val) const;
};

}  // namespace APBSystem