
#pragma once

#include <cstdint>
#include <set>
#include <vector>
#include "apb_types.hpp"  // 為了使用 APBSystem::CompleterID 或地址常數
namespace APBSystem {

class Statistics {
   public:
    Statistics();

    void record_paddr_bits(const CompleterID& completer_id, uint32_t paddr_value, bool paddr_has_x);
    void record_pwdata_bits(const CompleterID& completer_id, uint32_t pwdata_value, bool pwdata_has_x);
    // 記錄 APB 交易 (由 ApbAnalyzer 呼叫)
    // duration_pclk_edges: 交易持續的 PCLK 上升沿數量
    void record_read_transaction(bool had_wait_states, uint64_t duration_pclk_edges);
    void record_write_transaction(bool had_wait_states, uint64_t duration_pclk_edges);

    // 記錄 PCLK 週期類型 (由 ApbAnalyzer 或 main 在適當時機呼叫)
    void record_bus_active_pclk_edge();  // 每當 PSEL 為高且 PCLK 上升沿發生時

    void record_completer_access(uint32_t paddr);
    // 設定總體資訊 (由 main 呼叫)
    void set_total_pclk_rising_edges(uint64_t total_edges);
    void set_cpu_elapsed_time_ms(double time_ms);

    void set_bus_widths(int paddr_width, int pwdata_width);
    // --- Getters for ReportGenerator ---
    // 1. Number of Read Transactions with no wait states
    uint64_t get_read_transactions_no_wait() const;
    // 2. Number of Read Transactions with wait states
    uint64_t get_read_transactions_with_wait() const;
    // 3. Number of Write Transactions with no wait states
    uint64_t get_write_transactions_no_wait() const;
    // 4. Number of Write Transactions with wait states
    uint64_t get_write_transactions_with_wait() const;

    // 5. Average Read Cycle (以 PCLK 上升沿計數為單位)
    double get_average_read_cycle_duration() const;
    // 6. Average Write Cycle (以 PCLK 上升沿計數為單位)
    double get_average_write_cycle_duration() const;

    // 7. Bus Utilization (%)
    double get_bus_utilization_percentage() const;
    // 8. Number of Idle Cycles (PCLK 上升沿計數)
    uint64_t get_num_idle_pclk_edges() const;

    // 9. Number of Completers (指被存取過的獨立 Completer 數量)
    int get_number_of_unique_completers_accessed() const;
    // 10. CPU Elapsed Time (ms)
    double get_cpu_elapsed_time_ms() const;
    // get_accessed_completer_ids() 返回的是set，用於計算unique數量
    const std::set<CompleterID>& get_accessed_completer_ids_set() const;
    // 新增：返回按首次存取順序排列的 Completer 列表
    const std::vector<CompleterID>& get_ordered_accessed_completers() const;
    uint64_t get_total_pclk_edges() const;
    uint64_t get_total_bus_active_pclk_edges() const;
    const std::map<APBSystem::CompleterID, uint64_t>& get_completer_transaction_counts() const;
    const std::map<APBSystem::CompleterID, CompleterBitActivity>& get_completer_bit_activity_map() const;
    void set_first_valid_pclk_edge_for_stats(uint64_t first_valid_edge);

   private:
    uint64_t m_read_transactions_no_wait;
    uint64_t m_read_transactions_with_wait;
    uint64_t m_write_transactions_no_wait;
    uint64_t m_write_transactions_with_wait;

    uint64_t m_total_pclk_edges_for_read_transactions;
    uint64_t m_total_pclk_edges_for_write_transactions;

    uint64_t m_bus_active_pclk_edges;        // PSEL 為高時的 PCLK 上升沿計數
    uint64_t m_total_simulation_pclk_edges;  // VCD 中的總 PCLK 上升沿計數

    std::set<CompleterID> m_accessed_completer_ids_set;      // 儲存獨立的 Completer ID
    std::vector<CompleterID> m_ordered_accessed_completers;  // 按首次存取順序儲
    double m_cpu_elapsed_time_ms;

    uint64_t m_first_valid_pclk_edge_for_stats;  // 第一個有效的 PCLK 上升沿，用於統計計算

    std::map<APBSystem::CompleterID, uint64_t> m_completer_transaction_counts;
    std::map<APBSystem::CompleterID, CompleterBitActivity> m_completer_bit_activity_map;
};

}  // namespace APBSystem