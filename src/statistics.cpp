#include "statistics.hpp"
#include <algorithm>  // 用於 std::find
#include <limits>     // 用於處理除以零的情況
#include <set>
namespace APBSystem {

Statistics::Statistics()
    : m_read_transactions_no_wait(0),
      m_read_transactions_with_wait(0),
      m_write_transactions_no_wait(0),
      m_write_transactions_with_wait(0),
      m_total_pclk_edges_for_read_transactions(0),
      m_total_pclk_edges_for_write_transactions(0),
      m_bus_active_pclk_edges(0),
      m_total_simulation_pclk_edges(0),
      m_cpu_elapsed_time_ms(0.0),
      m_first_valid_pclk_edge_for_stats(0),
      m_out_of_range_access_count(0),
      m_timeout_error_count(0),
      m_data_corruption_count(0),
      m_mirroring_error_count(0) {}

void Statistics::record_read_transaction(bool had_wait_states, uint64_t duration_pclk_edges) {
    m_total_pclk_edges_for_read_transactions += duration_pclk_edges;
    if (had_wait_states) {
        m_read_transactions_with_wait++;
    } else {
        m_read_transactions_no_wait++;
    }
}

void Statistics::record_write_transaction(bool had_wait_states, uint64_t duration_pclk_edges) {
    m_total_pclk_edges_for_write_transactions += duration_pclk_edges;
    if (had_wait_states) {
        m_write_transactions_with_wait++;
    } else {
        m_write_transactions_no_wait++;
    }
}
void Statistics::record_timeout_error(const TransactionTimeoutDetail& detail) {
    m_timeout_error_count++;
    m_timeout_error_details.push_back(detail);
}

void Statistics::add_paddr_sample(CompleterID completer, uint32_t paddr_value) {
    if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
        return;

    // 確保 Completer 已被記錄
    record_accessed_completer(completer);

    m_completer_raw_data_samples[completer].paddr_samples.push_back(paddr_value);
}
void Statistics::add_pwdata_sample(CompleterID completer, uint32_t data_value) {
    if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
        return;

    record_accessed_completer(completer);

    m_completer_raw_data_samples[completer].pwdata_samples.push_back(data_value);
}

void Statistics::add_data_bus_sample(CompleterID completer, uint32_t data_value, bool data_has_x, bool is_write_op) {
    if (completer == CompleterID::NONE)
        return;
    if (m_completer_raw_data_samples.find(completer) == m_completer_raw_data_samples.end()) {
        m_completer_raw_data_samples[completer] = CompleterRawDataSamples();
    }
    if (m_completer_bit_activity_map.find(completer) == m_completer_bit_activity_map.end()) {  // 確保條目存在
        m_completer_bit_activity_map[completer] = CompleterBitActivity();
    }

    if (is_write_op && !data_has_x) {
        m_completer_raw_data_samples[completer].pwdata_samples.push_back(data_value);
    }
}
void Statistics::record_out_of_range_access(const OutOfRangeAccessDetail& detail) {
    m_out_of_range_access_count++;
    m_out_of_range_details.push_back(detail);
}
void Statistics::update_shadow_memory(CompleterID completer, uint32_t paddr, uint32_t pwdata, uint64_t timestamp) {
    if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
        return;

    m_shadow_memories[completer][paddr] = {pwdata, timestamp};

    m_reverse_write_lookup[pwdata] = {paddr, timestamp};
}

void Statistics::check_prdata_against_shadow_memory(CompleterID completer, uint32_t paddr, uint32_t prdata, uint64_t timestamp) {
    if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
        return;

    if (m_shadow_memories[completer].count(paddr)) {
        uint32_t expected_data = m_shadow_memories[completer][paddr].data;
        if (prdata != expected_data) {
            m_data_corruption_count++;
            m_data_integrity_error_details.push_back({timestamp, paddr, expected_data, prdata, completer, false, 0, 0});
        }
    } else {
        if (m_reverse_write_lookup.count(prdata)) {
            // 這個數據值之前確實被寫入過，但不是寫到當前地址 paddr
            const auto& original_write = m_reverse_write_lookup.at(prdata);  // 使用 .at() 獲取 const 引用
            // 確保原始寫入的地址與當前讀取地址不同
            if (original_write.address != paddr) {  // <-- 現在這行是正確的
                // 從未被寫入過的地址，卻讀出了已知數據 -> 數據鏡像 (Data Mirroring)
                m_mirroring_error_count++;
                m_data_integrity_error_details.push_back({timestamp, paddr, 0, prdata, completer,  // 預期為0或未定義
                                                          true, original_write.address, original_write.timestamp});
            }
        }
    }
}
void Statistics::record_bus_active_pclk_edge() {
    m_bus_active_pclk_edges++;
}
void Statistics::analyze_bus_shorts() {
    const int BITS_TO_ANALYZE = 8;

    // 使用 C++11 相容的迭代器
    for (std::map<CompleterID, CompleterRawDataSamples>::const_iterator it = m_completer_raw_data_samples.begin();
         it != m_completer_raw_data_samples.end(); ++it) {
        CompleterID comp_id = it->first;
        const CompleterRawDataSamples& samples_container = it->second;

        if (comp_id == CompleterID::UNKNOWN_COMPLETER || comp_id == CompleterID::NONE) {
            continue;
        }
        if (m_completer_bit_activity_map.find(comp_id) == m_completer_bit_activity_map.end()) {
            continue;
        }
        auto& bit_activity = m_completer_bit_activity_map.at(comp_id);

        // --- 分析 PADDR 短路 ---
        if (samples_container.paddr_samples.size() >= 2) {
            std::set<uint32_t> unique_samples(samples_container.paddr_samples.begin(), samples_container.paddr_samples.end());
            if (unique_samples.size() >= 2) {
                for (int i = 0; i < BITS_TO_ANALYZE; ++i) {
                    for (int j = i + 1; j < BITS_TO_ANALYZE; ++j) {
                        if (bit_activity.paddr_bit_details[i].status != BitConnectionStatus::CORRECT ||
                            bit_activity.paddr_bit_details[j].status != BitConnectionStatus::CORRECT) {
                            continue;
                        }

                        bool always_same = true;
                        for (std::set<uint32_t>::const_iterator sample_it = unique_samples.begin(); sample_it != unique_samples.end(); ++sample_it) {
                            uint32_t val = *sample_it;
                            bool val1 = (val >> i) & 0x1;
                            bool val2 = (val >> j) & 0x1;
                            if (val1 != val2) {
                                always_same = false;
                                break;
                            }
                        }

                        if (always_same) {
                            bit_activity.paddr_bit_details[i].status = BitConnectionStatus::SHORTED;
                            bit_activity.paddr_bit_details[i].shorted_with_bit_index = j;

                            bit_activity.paddr_bit_details[j].status = BitConnectionStatus::SHORTED;
                            bit_activity.paddr_bit_details[j].shorted_with_bit_index = i;
                        }
                    }
                }
            }
        }

        // --- 分析 PWDATA 短路 ---
        if (samples_container.pwdata_samples.size() >= 2) {
            std::set<uint32_t> unique_samples(samples_container.pwdata_samples.begin(), samples_container.pwdata_samples.end());
            if (unique_samples.size() >= 2) {
                for (int i = 0; i < BITS_TO_ANALYZE; ++i) {
                    for (int j = i + 1; j < BITS_TO_ANALYZE; ++j) {
                        if (bit_activity.pwdata_bit_details[i].status != BitConnectionStatus::CORRECT ||
                            bit_activity.pwdata_bit_details[j].status != BitConnectionStatus::CORRECT) {
                            continue;
                        }

                        bool always_same = true;
                        for (std::set<uint32_t>::const_iterator sample_it = unique_samples.begin(); sample_it != unique_samples.end(); ++sample_it) {
                            uint32_t val = *sample_it;
                            bool val1 = (val >> i) & 0x1;
                            bool val2 = (val >> j) & 0x1;
                            if (val1 != val2) {
                                always_same = false;
                                break;
                            }
                        }

                        if (always_same) {
                            bit_activity.pwdata_bit_details[i].status = BitConnectionStatus::SHORTED;
                            bit_activity.pwdata_bit_details[i].shorted_with_bit_index = j;

                            bit_activity.pwdata_bit_details[j].status = BitConnectionStatus::SHORTED;
                            bit_activity.pwdata_bit_details[j].shorted_with_bit_index = i;
                        }
                    }
                }
            }
        }
    }
}
void Statistics::record_accessed_completer(CompleterID completer_id) {
    if (completer_id == CompleterID::NONE)
        return;

    m_accessed_completer_ids_set.insert(completer_id);

    if (std::find(m_ordered_accessed_completers.begin(), m_ordered_accessed_completers.end(), completer_id) == m_ordered_accessed_completers.end()) {
        m_accessed_completer_ids_set.insert(completer_id);
        m_ordered_accessed_completers.push_back(completer_id);
    }

    if (m_completer_bit_activity_map.find(completer_id) == m_completer_bit_activity_map.end()) {
        m_completer_bit_activity_map[completer_id] = CompleterBitActivity();
    }
    if (m_completer_raw_data_samples.find(completer_id) == m_completer_raw_data_samples.end()) {
        m_completer_raw_data_samples[completer_id] = CompleterRawDataSamples();
    }
}

void Statistics::set_total_pclk_rising_edges(uint64_t total_edges) {
    m_total_simulation_pclk_edges = total_edges;
}

void Statistics::set_cpu_elapsed_time_ms(double time_ms) {
    m_cpu_elapsed_time_ms = time_ms;
}

void Statistics::set_first_valid_pclk_edge_for_stats(uint64_t first_valid_edge) {
    m_first_valid_pclk_edge_for_stats = first_valid_edge;
}

// --- Getters ---
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
    uint64_t total_reads = m_read_transactions_no_wait + m_read_transactions_with_wait;
    if (total_reads == 0)
        return 0.0;
    return static_cast<double>(m_total_pclk_edges_for_read_transactions) / total_reads;
}

double Statistics::get_average_write_cycle_duration() const {
    uint64_t total_writes = m_write_transactions_no_wait + m_write_transactions_with_wait;
    if (total_writes == 0)
        return 0.0;
    return static_cast<double>(m_total_pclk_edges_for_write_transactions) / total_writes;
}

double Statistics::get_bus_utilization_percentage() const {
    if (m_total_simulation_pclk_edges == 0)
        return 0.0;
    uint64_t effective_total_edges = 0;
    if (m_first_valid_pclk_edge_for_stats > 0 && m_first_valid_pclk_edge_for_stats <= m_total_simulation_pclk_edges) {
        effective_total_edges = m_total_simulation_pclk_edges - m_first_valid_pclk_edge_for_stats + 1;
    } else {
        // 如果 m_first_valid_pclk_edge_for_stats 無效 (例如一直處於復位，或VCD沒有PCLK上升沿)
        // 則有效分析週期為0，利用率也為0
        // 或者，如果測試案例期望此時 Bus Util 基於 VCD 總 PCLK 數，則用 m_total_simulation_pclk_edges
        // 但為了匹配 Idle Cycles 的調整，這裡應該讓分母也對應有效區間
        return 0.0;  // 或者如果 m_total_simulation_pclk_edges > 0 且 m_first_valid_pclk_edge_for_stats == 0
                     // 可以理解為從未脫離復位，所以有效利用率為0
    }

    if (effective_total_edges == 0)
        return 0.0;

    // m_bus_active_pclk_edges 已經是只計算復位結束後的活動了
    return (static_cast<double>(m_bus_active_pclk_edges) / effective_total_edges) * 100.0;
}

uint64_t Statistics::get_num_idle_pclk_edges() const {
    if (m_total_simulation_pclk_edges == 0)
        return 0;

    uint64_t effective_total_edges = 0;
    if (m_first_valid_pclk_edge_for_stats > 0 && m_first_valid_pclk_edge_for_stats <= m_total_simulation_pclk_edges) {
        // pclk_edge_count 從 1 開始計，所以 first_valid 是第 N 個邊沿
        // 例如 total=100, first_valid=21, effective = 100-21+1 = 80 (即第21到第100個邊沿)
        effective_total_edges = m_total_simulation_pclk_edges - m_first_valid_pclk_edge_for_stats + 1;
    } else {
        // 如果 m_first_valid_pclk_edge_for_stats 無效 (例如一直處於復位)
        // 則有效分析週期為0，Idle也為0
        // 或者，如果測試案例期望此時 Idle 是 m_total_simulation_pclk_edges - m_bus_active_pclk_edges
        // (這種情況下 bus_active 會是0，Idle 就是 m_total_simulation_pclk_edges)
        // 為了匹配，這裡應該返回0，因為沒有有效的分析區間
        return 0;  // 如果從未脫離復位，則有效Idle週期為0
    }

    if (effective_total_edges < m_bus_active_pclk_edges) {
        // std::cerr << "Warning: effective_total_edges < m_bus_active_pclk_edges in get_num_idle_pclk_edges. "
        //           << "Effective: " << effective_total_edges << ", Active: " << m_bus_active_pclk_edges
        //           << ", TotalSim: " << m_total_simulation_pclk_edges << ", FirstValid: " << m_first_valid_pclk_edge_for_stats << std::endl;
        return 0;  // 防禦性
    }
    return effective_total_edges - m_bus_active_pclk_edges;
}

int Statistics::get_number_of_unique_completers_accessed() const {
    return m_accessed_completer_ids_set.size();
}

double Statistics::get_cpu_elapsed_time_ms() const {
    return m_cpu_elapsed_time_ms;
}

uint64_t Statistics::get_total_pclk_edges() const {
    return m_total_simulation_pclk_edges;
}

uint64_t Statistics::get_total_bus_active_pclk_edges() const {
    return m_bus_active_pclk_edges;
}
const std::map<APBSystem::CompleterID, CompleterBitActivity>& Statistics::get_completer_bit_activity_map() const {
    return m_completer_bit_activity_map;
}
const std::vector<CompleterID>& Statistics::get_ordered_accessed_completers() const {
    return m_ordered_accessed_completers;
}
const std::vector<OutOfRangeAccessDetail>& Statistics::get_out_of_range_details() const {
    return m_out_of_range_details;
}
uint64_t Statistics::get_timeout_error_count() const {
    return m_timeout_error_count;
}
const std::vector<TransactionTimeoutDetail>& Statistics::get_timeout_error_details() const {
    return m_timeout_error_details;
}

uint64_t Statistics::get_data_corruption_count() const {
    return m_data_corruption_count;
}
uint64_t Statistics::get_mirroring_error_count() const {
    return m_mirroring_error_count;
}
const std::vector<DataIntegrityErrorDetail>& Statistics::get_data_integrity_error_details() const {
    return m_data_integrity_error_details;
}

}  // namespace APBSystem
