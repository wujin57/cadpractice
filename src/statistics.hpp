#pragma once

#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>
#include "apb_types.hpp"

namespace APBSystem {

class Statistics {
   public:
    Statistics();
    bool is_completer_corrupted(CompleterID cid);
    bool is_transaction_timeout(uint64_t start_time, uint32_t paddr) const;

    // --- 資料收集 ---
    void record_paddr_for_corruption_analysis(CompleterID completer, uint32_t paddr_value);
    void record_pwdata_for_corruption_analysis(CompleterID completer, uint32_t pwdata_value);
    void record_bus_active_pclk_edge();
    void record_accessed_completer(CompleterID completer_id);
    void update_shadow_memory(CompleterID completer, uint32_t paddr, uint32_t pwdata, uint64_t timestamp);
    void check_for_data_mirroring(CompleterID completer, uint32_t paddr, uint32_t prdata, uint64_t timestamp);
    void record_read_transaction(bool had_wait_states, uint64_t duration_pclk_edges);
    void record_write_transaction(bool had_wait_states, uint64_t duration_pclk_edges);

    // --- 錯誤記錄 ---
    void record_out_of_range_access(const OutOfRangeAccessDetail& detail);
    void record_timeout_error(const TransactionTimeoutDetail& detail);
    void record_read_write_overlap_error(const ReadWriteOverlapDetail& detail);
    void record_data_mirroring(const DataMirroringDetail& d);

    // --- 分析與設定 ---
    void set_bus_widths(int paddr_width, int pwdata_width);
    void set_total_pclk_rising_edges(uint64_t total_edges);
    void set_cpu_elapsed_time_ms(double time_ms);
    void set_first_valid_pclk_edge_for_stats(uint64_t first_valid_edge);
    void finalize_bit_activity();

    // --- Getters ---
    uint64_t get_read_transactions_no_wait() const;
    uint64_t get_read_transactions_with_wait() const;
    uint64_t get_write_transactions_no_wait() const;
    uint64_t get_write_transactions_with_wait() const;
    double get_average_read_cycle_duration() const;
    double get_average_write_cycle_duration() const;
    double get_bus_utilization_percentage() const;
    uint64_t get_num_idle_pclk_edges() const;
    int get_number_of_unique_completers_accessed() const;
    double get_cpu_elapsed_time_ms() const;
    const std::vector<OutOfRangeAccessDetail>& get_out_of_range_details() const;
    const std::vector<TransactionTimeoutDetail>& get_timeout_error_details() const;
    const std::vector<ReadWriteOverlapDetail>& get_read_write_overlap_details() const;
    const std::vector<DataMirroringDetail>& get_data_mirroring_details() const;
    uint64_t get_mirroring_error_count() const;
    const std::vector<CompleterID>& get_ordered_accessed_completers() const;
    const std::unordered_map<APBSystem::CompleterID, CompleterBitActivity>& get_completer_bit_activity_map() const;

   private:
    uint64_t m_read_transactions_no_wait, m_read_transactions_with_wait;
    uint64_t m_write_transactions_no_wait, m_write_transactions_with_wait;
    uint64_t m_total_pclk_edges_for_read_transactions, m_total_pclk_edges_for_write_transactions;
    uint64_t m_bus_active_pclk_edges, m_total_simulation_pclk_edges;
    double m_cpu_elapsed_time_ms;
    uint64_t m_first_valid_pclk_edge_for_stats;

    int m_paddr_width{32}, m_pwdata_width{32};
    std::set<CompleterID> m_accessed_completer_ids_set;
    std::vector<CompleterID> m_ordered_accessed_completers;

    std::unordered_map<APBSystem::CompleterID, CompleterBitActivity> m_completer_bit_activity_map;

    std::vector<OutOfRangeAccessDetail> m_out_of_range_details;
    std::vector<TransactionTimeoutDetail> m_timeout_error_details;
    std::vector<ReadWriteOverlapDetail> m_read_write_overlap_details;
    std::vector<DataMirroringDetail> m_data_mirroring_details;

    struct ShadowMemoryEntry {
        uint32_t data;
        uint64_t timestamp;
    };
    std::unordered_map<CompleterID, std::unordered_map<uint32_t, ShadowMemoryEntry>> m_shadow_memories;
    std::unordered_map<uint32_t, ReverseWriteInfo> m_reverse_write_lookup;
};

}  // namespace APBSystem