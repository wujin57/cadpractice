
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
    void record_bus_active_pclk_edge();
    void record_accessed_completer(CompleterID completer_id);
    void add_data_bus_sample(CompleterID completer, uint32_t data_value, bool data_has_x, bool is_write_op);

    void update_shadow_memory(CompleterID completer, uint32_t paddr, uint32_t pwdata, uint64_t timestamp);
    void check_prdata_against_shadow_memory(CompleterID completer, uint32_t paddr, uint32_t prdata, uint64_t timestamp);
    void record_paddr_for_error_check(CompleterID completer, uint32_t paddr_value, bool paddr_has_x);
    void record_pwdata_for_error_check(CompleterID completer, uint32_t pwdata_value, bool pwdata_has_x);

    void record_out_of_range_access(const OutOfRangeAccessDetail& detail);

    void analyze_bus_shorts();

    void record_read_transaction(bool had_wait_states, uint64_t duration_pclk_edges);
    void record_write_transaction(bool had_wait_states, uint64_t duration_pclk_edges);

    void record_timeout_error(const TransactionTimeoutDetail& detail);

    void record_completer_access(uint32_t paddr);
    void record_read_write_overlap_error(const ReadWriteOverlapDetail& detail);
    void set_bus_widths(int paddr_width, int pwdata_width);
    void set_total_pclk_rising_edges(uint64_t total_edges);
    void set_cpu_elapsed_time_ms(double time_ms);

    // --- Getters for ReportGenerator ---
    // 1. Number of Read Transactions with no wait states
    uint64_t get_read_transactions_no_wait() const;
    // 2. Number of Read Transactions with wait states
    uint64_t get_read_transactions_with_wait() const;
    // 3. Number of Write Transactions with no wait states
    uint64_t get_write_transactions_no_wait() const;
    // 4. Number of Write Transactions with wait states
    uint64_t get_write_transactions_with_wait() const;

    double get_average_read_cycle_duration() const;
    double get_average_write_cycle_duration() const;

    // 7. Bus Utilization (%)
    double get_bus_utilization_percentage() const;
    uint64_t get_num_idle_pclk_edges() const;

    int get_number_of_unique_completers_accessed() const;
    // 10. CPU Elapsed Time (ms)
    double get_cpu_elapsed_time_ms() const;

    uint64_t get_out_of_range_access_count() const;
    const std::vector<OutOfRangeAccessDetail>& get_out_of_range_details() const;
    const std::set<CompleterID>& get_accessed_completer_ids_set() const;
    const std::vector<CompleterID>& get_ordered_accessed_completers() const;
    uint64_t get_total_pclk_edges() const;
    uint64_t get_total_bus_active_pclk_edges() const;
    uint64_t get_timeout_error_count() const;
    uint64_t get_data_corruption_count() const;
    uint64_t get_mirroring_error_count() const;
    const std::vector<DataIntegrityErrorDetail>& get_data_integrity_error_details() const;
    const std::vector<TransactionTimeoutDetail>& get_timeout_error_details() const;
    const std::map<APBSystem::CompleterID, uint64_t>& get_completer_transaction_counts() const;
    const std::map<APBSystem::CompleterID, CompleterBitActivity>& get_completer_bit_activity_map() const;
    void set_first_valid_pclk_edge_for_stats(uint64_t first_valid_edge);

    uint64_t get_read_write_overlap_count() const;
    const std::vector<ReadWriteOverlapDetail>& get_read_write_overlap_details() const;

   private:
    uint64_t m_read_transactions_no_wait;
    uint64_t m_read_transactions_with_wait;
    uint64_t m_write_transactions_no_wait;
    uint64_t m_write_transactions_with_wait;
    uint64_t m_total_pclk_edges_for_read_transactions;
    uint64_t m_total_pclk_edges_for_write_transactions;
    uint64_t m_bus_active_pclk_edges;
    uint64_t m_total_simulation_pclk_edges;

    int m_paddr_width{32};
    int m_pwdata_width{32};
    std::set<CompleterID> m_accessed_completer_ids_set;
    std::vector<CompleterID> m_ordered_accessed_completers;

    double m_cpu_elapsed_time_ms;
    uint64_t m_first_valid_pclk_edge_for_stats;
    uint64_t m_out_of_range_access_count;
    std::vector<OutOfRangeAccessDetail> m_out_of_range_details;
    std::map<APBSystem::CompleterID, uint64_t> m_completer_transaction_counts;
    std::map<APBSystem::CompleterID, CompleterBitActivity> m_completer_bit_activity_map;

    uint64_t m_timeout_error_count;
    std::vector<TransactionTimeoutDetail> m_timeout_error_details;

    // 新增：影子記憶體相關成員
    // 外層 map: CompleterID -> 該 Completer 的影子記憶體
    // 內層 map: address -> {data, timestamp}
    struct ShadowMemoryEntry {
        uint32_t data;
        uint64_t timestamp;
    };
    std::map<CompleterID, std::map<uint32_t, ShadowMemoryEntry>> m_shadow_memories;

    uint64_t m_data_corruption_count;
    uint64_t m_mirroring_error_count;
    std::vector<DataIntegrityErrorDetail> m_data_integrity_error_details;

    // key: data value, value: {address, timestamp} of last write
    std::map<uint32_t, ReverseWriteInfo> m_reverse_write_lookup;

    uint64_t m_read_write_overlap_count;
    std::vector<ReadWriteOverlapDetail> m_read_write_overlap_details;
};

}  // namespace APBSystem