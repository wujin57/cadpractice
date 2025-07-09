#include "statistics.hpp"
#include <algorithm>
#include <iostream>
#include <set>

namespace APBSystem {

Statistics::Statistics()
    : m_read_transactions_no_wait(0), m_read_transactions_with_wait(0), m_write_transactions_no_wait(0), m_write_transactions_with_wait(0), m_total_pclk_edges_for_read_transactions(0), m_total_pclk_edges_for_write_transactions(0), m_bus_active_pclk_edges(0), m_total_simulation_pclk_edges(0), m_cpu_elapsed_time_ms(0.0), m_first_valid_pclk_edge_for_stats(0) {}

void Statistics::record_accessed_completer(CompleterID completer_id) {
    if (completer_id == CompleterID::NONE || completer_id == CompleterID::UNKNOWN_COMPLETER)
        return;
    if (m_completer_bit_activity_map.find(completer_id) == m_completer_bit_activity_map.end()) {
        m_accessed_completer_ids_set.insert(completer_id);
        m_ordered_accessed_completers.push_back(completer_id);
        CompleterBitActivity activity;
        activity.resize(m_paddr_width, m_pwdata_width);
        m_completer_bit_activity_map[completer_id] = activity;
    }
}

void Statistics::record_paddr_for_corruption_analysis(CompleterID completer, uint32_t paddr_value) {
    if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
        return;
    auto& activity = m_completer_bit_activity_map.at(completer);
    for (int i = 0; i < m_paddr_width; ++i) {
        for (int j = i + 1; j < m_paddr_width; ++j) {
            bool bit_i_val = (paddr_value >> i) & 1;
            bool bit_j_val = (paddr_value >> j) & 1;
            int combination_idx = (bit_i_val << 1) | bit_j_val;
            activity.paddr_combinations[i][j][combination_idx]++;
        }
    }
}

void Statistics::record_pwdata_for_corruption_analysis(CompleterID completer, uint32_t pwdata_value) {
    if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
        return;
    auto& activity = m_completer_bit_activity_map.at(completer);
    for (int i = 0; i < m_pwdata_width; ++i) {
        for (int j = i + 1; j < m_pwdata_width; ++j) {
            bool bit_i_val = (pwdata_value >> i) & 1;
            bool bit_j_val = (pwdata_value >> j) & 1;
            int combination_idx = (bit_i_val << 1) | bit_j_val;
            activity.pwdata_combinations[i][j][combination_idx]++;
        }
    }
}

void Statistics::check_for_data_mirroring(CompleterID completer, uint32_t paddr, uint32_t prdata, uint64_t timestamp) {
    if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
        return;
    static const std::set<uint32_t> externally_driven_regs = {
        0x1A101008, 0x1A100014};
    if (externally_driven_regs.count(paddr))
        return;
    if (m_shadow_memories[completer].count(paddr))
        return;
    if (m_reverse_write_lookup.count(prdata)) {
        const auto& original_write = m_reverse_write_lookup.at(prdata);
        if (original_write.address != paddr) {
            record_data_mirroring({timestamp, paddr, prdata, original_write.address, original_write.timestamp});
        }
    }
}

void Statistics::finalize_bit_activity() {
    const int MIN_EVIDENCE_COUNT = 1;

    for (auto& kv : m_completer_bit_activity_map) {
        auto& act = kv.second;

        // --- PADDR 分析 ---
        {
            // 階段一：找出所有「潛在的」短路候選對
            std::vector<std::pair<int, int>> candidate_pairs;
            for (int i = 0; i < m_paddr_width; ++i) {
                for (int j = i + 1; j < m_paddr_width; ++j) {
                    const auto& counts = act.paddr_combinations[i][j];
                    bool has_independent_evidence = (counts[1] > 0) || (counts[2] > 0);
                    bool has_sufficient_sync_evidence = (counts[0] >= MIN_EVIDENCE_COUNT) && (counts[3] >= MIN_EVIDENCE_COUNT);

                    if (!has_independent_evidence && has_sufficient_sync_evidence) {
                        candidate_pairs.push_back({i, j});
                    }
                }
            }

            // 階段二：應用「最多一對短路」的全局約束
            if (candidate_pairs.size() == 1) {
                int bit_a = candidate_pairs[0].first;
                int bit_b = candidate_pairs[0].second;
                act.paddr_bit_details[bit_a].status = BitConnectionStatus::SHORTED;
                act.paddr_bit_details[bit_a].shorted_with_bit_index = bit_b;
                act.paddr_bit_details[bit_b].status = BitConnectionStatus::SHORTED;
                act.paddr_bit_details[bit_b].shorted_with_bit_index = bit_a;
            }
            // 如果候選對為 0 或大於 1，則不做任何事，保持所有位元為 CORRECT
        }

        // --- PWDATA 分析 (邏輯完全相同) ---
        {
            std::vector<std::pair<int, int>> candidate_pairs;
            for (int i = 0; i < m_pwdata_width; ++i) {
                for (int j = i + 1; j < m_pwdata_width; ++j) {
                    const auto& counts = act.pwdata_combinations[i][j];
                    bool has_independent_evidence = (counts[1] > 0) || (counts[2] > 0);
                    bool has_sufficient_sync_evidence = (counts[0] >= MIN_EVIDENCE_COUNT) && (counts[3] >= MIN_EVIDENCE_COUNT);

                    if (!has_independent_evidence && has_sufficient_sync_evidence) {
                        candidate_pairs.push_back({i, j});
                    }
                }
            }

            if (candidate_pairs.size() == 1) {
                int bit_a = candidate_pairs[0].first;
                int bit_b = candidate_pairs[0].second;
                act.pwdata_bit_details[bit_a].status = BitConnectionStatus::SHORTED;
                act.pwdata_bit_details[bit_a].shorted_with_bit_index = bit_b;
                act.pwdata_bit_details[bit_b].status = BitConnectionStatus::SHORTED;
                act.pwdata_bit_details[bit_b].shorted_with_bit_index = bit_a;
            }
        }
    }
}

// --- 其他記錄與設定函式 (此處省略以保持簡潔) ---
void Statistics::record_read_transaction(bool h, uint64_t d) {
    if (h)
        m_read_transactions_with_wait++;
    else
        m_read_transactions_no_wait++;
    m_total_pclk_edges_for_read_transactions += d;
}
void Statistics::record_write_transaction(bool h, uint64_t d) {
    if (h)
        m_write_transactions_with_wait++;
    else
        m_write_transactions_no_wait++;
    m_total_pclk_edges_for_write_transactions += d;
}
void Statistics::record_out_of_range_access(const OutOfRangeAccessDetail& d) {
    m_out_of_range_details.push_back(d);
}
void Statistics::record_timeout_error(const TransactionTimeoutDetail& d) {
    m_timeout_error_details.push_back(d);
}
void Statistics::record_read_write_overlap_error(const ReadWriteOverlapDetail& d) {
    m_read_write_overlap_details.push_back(d);
}
void Statistics::record_address_corruption(const AddressCorruptionDetail& d) {
    m_address_corruption_details.push_back(d);
}
void Statistics::record_data_corruption(const DataCorruptionDetail& d) {
    m_data_corruption_details.push_back(d);
}
void Statistics::record_data_mirroring(const DataMirroringDetail& d) {
    m_data_mirroring_details.push_back(d);
}
void Statistics::update_shadow_memory(CompleterID c, uint32_t p, uint32_t d, uint64_t t) {
    if (c == CompleterID::NONE || c == CompleterID::UNKNOWN_COMPLETER)
        return;
    m_shadow_memories[c][p] = {d, t};
    m_reverse_write_lookup[d] = {p, t};
}
void Statistics::record_bus_active_pclk_edge() {
    m_bus_active_pclk_edges++;
}
void Statistics::set_bus_widths(int p, int d) {
    m_paddr_width = p > 0 ? p : 32;
    m_pwdata_width = d > 0 ? d : 32;
}
void Statistics::set_total_pclk_rising_edges(uint64_t t) {
    m_total_simulation_pclk_edges = t;
}
void Statistics::set_cpu_elapsed_time_ms(double t) {
    m_cpu_elapsed_time_ms = t;
}
void Statistics::set_first_valid_pclk_edge_for_stats(uint64_t e) {
    m_first_valid_pclk_edge_for_stats = e;
}

// --- Getters (此處省略以保持簡潔) ---
uint64_t Statistics::get_read_transactions_no_wait() const {
    return m_read_transactions_no_wait;
}
uint64_t Statistics::get_read_transactions_with_wait() const {
    return m_read_transactions_with_wait;
}
uint64_t Statistics::get_write_transactions_no_wait() const {
    return m_write_transactions_no_wait;
}
uint64_t Statistics::get_write_transactions_with_wait() const {
    return m_write_transactions_with_wait;
}
double Statistics::get_average_read_cycle_duration() const {
    uint64_t t = m_read_transactions_no_wait + m_read_transactions_with_wait;
    return t == 0 ? 0.0 : static_cast<double>(m_total_pclk_edges_for_read_transactions) / t;
}
double Statistics::get_average_write_cycle_duration() const {
    uint64_t t = m_write_transactions_no_wait + m_write_transactions_with_wait;
    return t == 0 ? 0.0 : static_cast<double>(m_total_pclk_edges_for_write_transactions) / t;
}
double Statistics::get_bus_utilization_percentage() const {
    if (m_total_simulation_pclk_edges == 0)
        return 0.0;
    uint64_t e = 0;
    if (m_first_valid_pclk_edge_for_stats > 0 && m_first_valid_pclk_edge_for_stats <= m_total_simulation_pclk_edges) {
        e = m_total_simulation_pclk_edges - m_first_valid_pclk_edge_for_stats + 1;
    } else if (m_first_valid_pclk_edge_for_stats == 0 && m_total_simulation_pclk_edges > 0) {
        return 0.0;
    } else {
        e = m_total_simulation_pclk_edges;
    }
    if (e == 0)
        return 0.0;
    return (static_cast<double>(m_bus_active_pclk_edges) / e) * 100.0;
}
uint64_t Statistics::get_num_idle_pclk_edges() const {
    if (m_total_simulation_pclk_edges == 0)
        return 0;
    uint64_t e = 0;
    if (m_first_valid_pclk_edge_for_stats > 0 && m_first_valid_pclk_edge_for_stats <= m_total_simulation_pclk_edges) {
        e = m_total_simulation_pclk_edges - m_first_valid_pclk_edge_for_stats + 1;
    } else if (m_first_valid_pclk_edge_for_stats == 0 && m_total_simulation_pclk_edges > 0) {
        return 0;
    } else {
        e = m_total_simulation_pclk_edges;
    }
    if (e < m_bus_active_pclk_edges)
        return 0;
    return e - m_bus_active_pclk_edges;
}
int Statistics::get_number_of_unique_completers_accessed() const {
    return m_accessed_completer_ids_set.size();
}
double Statistics::get_cpu_elapsed_time_ms() const {
    return m_cpu_elapsed_time_ms;
}
const std::vector<OutOfRangeAccessDetail>& Statistics::get_out_of_range_details() const {
    return m_out_of_range_details;
}
const std::vector<TransactionTimeoutDetail>& Statistics::get_timeout_error_details() const {
    return m_timeout_error_details;
}
const std::vector<ReadWriteOverlapDetail>& Statistics::get_read_write_overlap_details() const {
    return m_read_write_overlap_details;
}
const std::vector<AddressCorruptionDetail>& Statistics::get_address_corruption_details() const {
    return m_address_corruption_details;
}
const std::vector<DataCorruptionDetail>& Statistics::get_data_corruption_details() const {
    return m_data_corruption_details;
}
const std::vector<DataMirroringDetail>& Statistics::get_data_mirroring_details() const {
    return m_data_mirroring_details;
}
uint64_t Statistics::get_mirroring_error_count() const {
    return m_data_mirroring_details.size();
}
const std::vector<CompleterID>& Statistics::get_ordered_accessed_completers() const {
    return m_ordered_accessed_completers;
}
const std::map<APBSystem::CompleterID, CompleterBitActivity>& Statistics::get_completer_bit_activity_map() const {
    return m_completer_bit_activity_map;
}

}  // namespace APBSystem
