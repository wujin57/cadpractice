// apb_analyzer.hpp
#pragma once

#include <map>
#include <unordered_map>
#include "apb_types.hpp"
#include "statistics.hpp"
namespace APBSystem {

// 這個結構體用於追蹤待處理寫入的生命週期
struct PendingWriteInfo {
    uint64_t transaction_start_time_ps;
    uint64_t start_pclk_edge_count;
};

class ApbAnalyzer {
   public:
    ApbAnalyzer(Statistics& statistics);
    void analyze_on_pclk_rising_edge(const SignalState& current_snapshot, uint64_t pclk_edge_count);
    void finalize_analysis(uint64_t final_vcd_timestamp_ps);

   private:
    Statistics& m_statistics;

    ApbFsmState m_current_apb_fsm_state;
    TransactionInfo m_current_transaction;

    std::unordered_map<uint32_t, PendingWriteInfo> m_pending_writes;

    uint64_t m_current_pclk_edge_count;
    uint64_t m_transaction_cycle_counter;
    bool m_system_out_of_reset;
    uint64_t m_first_valid_pclk_edge_for_stats;

    void handle_idle_state(const SignalState& snapshot);
    void handle_setup_state(const SignalState& snapshot);
    void handle_access_state(const SignalState& snapshot);

    bool check_for_timeout(const SignalState& current_snapshot);
    void process_transaction_completion(const SignalState& snapshot_at_completion);

    CompleterID get_completer_id_from_paddr(uint32_t paddr) const;
    void check_for_out_of_range(const SignalState& snapshot_at_completion);
};
}  // namespace APBSystem
