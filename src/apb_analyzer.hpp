// apb_analyzer.hpp
#pragma once

#include <map>
#include <vector>
#include "apb_types.hpp"
#include "statistics.hpp"
namespace APBSystem {

class ApbAnalyzer {
   public:
    explicit ApbAnalyzer(Statistics& statistics /* std::ostream& debug_stream*/);

    void analyze_on_pclk_rising_edge(const SignalState& current_snapshot, uint64_t pclk_edge_count);
    void finalize_analysis(uint64_t final_vcd_timestamp);
    uint64_t get_completed_transaction_count() const {
        return m_completed_transaction_count;
    }

   private:
    void handle_idle_state(const SignalState& snapshot);
    void handle_setup_state(const SignalState& snapshot);
    void handle_access_state(const SignalState& snapshot);

    void process_transaction_completion(const SignalState& snapshot_at_completion);

    bool check_for_timeout(const SignalState& current_snapshot);
    void preliminary_check_for_out_of_range(const SignalState& snapshot_at_completion);
    void filter_and_commit_errors();

    CompleterID get_completer_id_from_paddr(uint32_t paddr) const;

    Statistics& m_statistics;
    ApbFsmState m_current_apb_fsm_state;
    TransactionInfo m_current_transaction;
    uint64_t m_current_pclk_edge_count;
    bool m_system_out_of_reset;
    uint64_t m_first_valid_pclk_edge_for_stats;
    uint64_t m_transaction_cycle_counter;

    struct PendingWriteInfo {
        uint64_t start_time_ps;
        uint64_t start_pclk_edge_count;
    };
    std::map<uint32_t, PendingWriteInfo> m_pending_writes;

    std::vector<TransactionInfo> m_completed_transactions;
    uint64_t m_completed_transaction_count;

    struct PreliminaryOverlapInfo {
        ReadWriteOverlapDetail detail;
        uint64_t write_start_time;
        uint32_t write_paddr;
    };
    std::vector<OutOfRangeAccessDetail> m_preliminary_oor_errors;
    std::vector<PreliminaryOverlapInfo> m_preliminary_overlap_errors;
    // std::ostream& m_debug_stream;
};

}  // namespace APBSystem