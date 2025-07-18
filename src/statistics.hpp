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

    inline void record_paddr_sample(CompleterID completer, uint32_t paddr_value) {
        if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
            return;
        m_paddr_samples[completer].push_back(paddr_value);
    }

    inline void record_pwdata_sample(CompleterID completer, uint32_t pwdata_value) {
        if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
            return;
        m_pwdata_samples[completer].push_back(pwdata_value);
    }

    inline CompleterBitActivity& ensure_activity(CompleterID completer_id) {
        // 處理無效 ID 的邊界情況，確保穩健性
        if (completer_id == CompleterID::NONE || completer_id == CompleterID::UNKNOWN_COMPLETER) {
            static CompleterBitActivity empty_activity;
            if (empty_activity.paddr_bit_details.empty() && m_paddr_width > 0) {
                empty_activity.resize(m_paddr_width, m_pwdata_width);
            }
            return empty_activity;
        }

        // 核心邏輯：一次性完成查找或建立
        auto it = m_completer_bit_activity_map.find(completer_id);
        if (it == m_completer_bit_activity_map.end()) {
            // 若還沒見過，就一次性完成所有初始化
            if (m_accessed_completer_ids_set.find(completer_id) == m_accessed_completer_ids_set.end()) {
                m_accessed_completer_ids_set.insert(completer_id);
                m_ordered_accessed_completers.push_back(completer_id);
            }

            CompleterBitActivity tmp;
            tmp.resize(m_paddr_width, m_pwdata_width);
            it = m_completer_bit_activity_map.emplace(completer_id, std::move(tmp)).first;
        }
        return it->second;
    }
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
    void record_address_corruption(const AddressCorruptionDetail& d);
    void record_data_corruption(const DataCorruptionDetail& d);
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
    const std::vector<AddressCorruptionDetail>& get_address_corruption_details() const;
    const std::vector<DataCorruptionDetail>& get_data_corruption_details() const;
    const std::vector<DataMirroringDetail>& get_data_mirroring_details() const;

    uint64_t get_mirroring_error_count() const;
    const std::vector<CompleterID>& get_ordered_accessed_completers() const;
    const std::map<APBSystem::CompleterID, CompleterBitActivity>& get_completer_bit_activity_map() const;

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
    std::map<APBSystem::CompleterID, CompleterBitActivity> m_completer_bit_activity_map;

    std::unordered_map<CompleterID, std::vector<uint32_t>> m_paddr_samples;
    std::unordered_map<CompleterID, std::vector<uint32_t>> m_pwdata_samples;
    std::vector<OutOfRangeAccessDetail> m_out_of_range_details;
    std::vector<TransactionTimeoutDetail> m_timeout_error_details;
    std::vector<ReadWriteOverlapDetail> m_read_write_overlap_details;
    std::vector<AddressCorruptionDetail> m_address_corruption_details;
    std::vector<DataCorruptionDetail> m_data_corruption_details;
    std::vector<DataMirroringDetail> m_data_mirroring_details;

    struct ShadowMemoryEntry {
        uint32_t data;
        uint64_t timestamp;
    };
    std::map<CompleterID, std::map<uint32_t, ShadowMemoryEntry>> m_shadow_memories;
    std::map<uint32_t, ReverseWriteInfo> m_reverse_write_lookup;
};

}  // namespace APBSystem
