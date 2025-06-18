#include "statistics.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
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
      m_read_write_overlap_count(0),
      m_data_corruption_count(0),
      m_mirroring_error_count(0) {}

void Statistics::set_bus_widths(int paddr_width, int pwdata_width) {
    m_paddr_width = (paddr_width > 0) ? paddr_width : 32;
    m_pwdata_width = (pwdata_width > 0) ? pwdata_width : 32;
}

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

void Statistics::record_read_write_overlap_error(const ReadWriteOverlapDetail& detail) {
    m_read_write_overlap_count++;
    m_read_write_overlap_details.push_back(detail);
}

void Statistics::update_shadow_memory(CompleterID completer, uint32_t paddr, uint32_t pwdata, uint64_t timestamp) {
    if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
        return;
    if (paddr == 0x1a100000) {
        /* std::cout << "[!!! UART_TARGET_WRITE !!! #" << timestamp << "] "
                   << "Updating Shadow Memory for PADDR=0x" << std::hex << paddr
                   << " with PWDATA=0x" << pwdata << std::dec << std::endl;*/
    }
    m_shadow_memories[completer][paddr] = {pwdata, timestamp};
    m_reverse_write_lookup[pwdata] = {paddr, timestamp};
}

void Statistics::add_data_bus_sample(CompleterID completer, uint32_t data_value, bool data_has_x, bool is_write_op) {
    if (completer == CompleterID::NONE)
        return;
    if (m_completer_bit_activity_map.find(completer) == m_completer_bit_activity_map.end()) {  // 確保條目存在
        m_completer_bit_activity_map[completer] = CompleterBitActivity();
    }
}
void Statistics::record_out_of_range_access(const OutOfRangeAccessDetail& detail) {
    m_out_of_range_access_count++;
    m_out_of_range_details.push_back(detail);
}

void Statistics::check_prdata_against_shadow_memory(CompleterID completer, uint32_t paddr, uint32_t prdata, uint64_t timestamp) {
    if (completer == CompleterID::NONE || completer == CompleterID::UNKNOWN_COMPLETER)
        return;
    static const std::set<uint32_t> externally_driven_regs = {
        0x1A101008,  // GPIO PADIN (Direct Input Mapping)
        0x1A100000,  // UART RBR (Buffered Input Data)
        0x1A100014,
    };

    if (externally_driven_regs.count(paddr)) {
        return;
    }

    if (m_shadow_memories[completer].count(paddr)) {
        uint32_t expected_data = m_shadow_memories[completer][paddr].data;
        /*std::cout << "[DEBUG #" << timestamp << "] "
                  << " -> Shadow Memory HIT! Expected PRDATA: 0x" << std::hex << expected_data << std::dec << std::endl;*/
        if (prdata != expected_data) {
            m_data_corruption_count++;
            m_data_integrity_error_details.push_back({timestamp, paddr, expected_data, prdata, completer, false, 0, 0});
            /* std::cout << "[DEBUG #" << timestamp << "] "
                       << " -> Shadow Memory HIT! Expected PRDATA: 0x" << std::hex << expected_data << std::dec << std::endl;*/
            uint32_t data_diff = expected_data ^ prdata;
            if (__builtin_popcount(data_diff) == 2) {
                int first_bit = -1, second_bit = -1;
                for (int i = 0; i < m_pwdata_width; ++i) {
                    if ((data_diff >> i) & 0x1) {
                        if (first_bit == -1)
                            first_bit = i;
                        else
                            second_bit = i;
                    }
                }

                if (first_bit != -1 && second_bit != -1) {
                    if (m_completer_bit_activity_map.count(completer)) {
                        auto& bit_activity = m_completer_bit_activity_map.at(completer);
                        if (bit_activity.pwdata_bit_details[first_bit].status == BitConnectionStatus::CORRECT) {
                            bit_activity.pwdata_bit_details[first_bit].status = BitConnectionStatus::SHORTED;
                            bit_activity.pwdata_bit_details[first_bit].shorted_with_bit_index = second_bit;
                        }
                        if (bit_activity.pwdata_bit_details[second_bit].status == BitConnectionStatus::CORRECT) {
                            bit_activity.pwdata_bit_details[second_bit].status = BitConnectionStatus::SHORTED;
                            bit_activity.pwdata_bit_details[second_bit].shorted_with_bit_index = first_bit;
                        }
                    }
                }
            }
        }
    } else {
        if (m_reverse_write_lookup.count(prdata)) {
            // --- 情況 2: 該地址之前從未被寫入過 (數據鏡像) ---
            if (m_reverse_write_lookup.count(prdata)) {
                const auto& original_write = m_reverse_write_lookup.at(prdata);
                if (original_write.address != paddr) {
                    m_mirroring_error_count++;
                    m_data_integrity_error_details.push_back({timestamp, paddr, 0, prdata, completer,
                                                              true, original_write.address, original_write.timestamp});
                }
            }
        }
    }
}
void Statistics::record_bus_active_pclk_edge() {
    m_bus_active_pclk_edges++;
}

void Statistics::record_accessed_completer(CompleterID completer_id) {
    if (completer_id == CompleterID::NONE || completer_id == CompleterID::UNKNOWN_COMPLETER)
        return;

    m_accessed_completer_ids_set.insert(completer_id);

    if (std::find(m_ordered_accessed_completers.begin(), m_ordered_accessed_completers.end(), completer_id) == m_ordered_accessed_completers.end()) {
        m_accessed_completer_ids_set.insert(completer_id);
        m_ordered_accessed_completers.push_back(completer_id);
    }

    if (m_completer_bit_activity_map.find(completer_id) == m_completer_bit_activity_map.end()) {
        CompleterBitActivity activity;
        activity.paddr_bit_details.resize(m_paddr_width, BitDetailStatus());
        activity.pwdata_bit_details.resize(m_pwdata_width, BitDetailStatus());
        m_completer_bit_activity_map[completer_id] = activity;
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
        effective_total_edges = m_total_simulation_pclk_edges - m_first_valid_pclk_edge_for_stats + 1;
    } else {
        return 0;
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
uint64_t Statistics::get_read_write_overlap_count() const {
    return m_read_write_overlap_count;
}

const std::vector<ReadWriteOverlapDetail>& Statistics::get_read_write_overlap_details() const {
    return m_read_write_overlap_details;
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
uint64_t Statistics::get_out_of_range_access_count() const {
    return m_out_of_range_access_count;
}
}  // namespace APBSystem
