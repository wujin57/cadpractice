#include "statistics.hpp"
#include <algorithm>  // 用於 std::find
#include <limits>     // 用於處理除以零的情況
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
      m_first_valid_pclk_edge_for_stats(0) {}

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

void Statistics::record_bus_active_pclk_edge() {
    m_bus_active_pclk_edges++;
}

void Statistics::record_completer_access(uint32_t paddr) {
    CompleterID current_completer;
    if (paddr >= UART_BASE_ADDR && paddr <= UART_END_ADDR) {
        current_completer = CompleterID::UART;
    } else if (paddr >= GPIO_BASE_ADDR && paddr <= GPIO_END_ADDR) {
        current_completer = CompleterID::GPIO;
    } else if (paddr >= SPI_MASTER_BASE_ADDR && paddr <= SPI_MASTER_END_ADDR) {
        current_completer = CompleterID::SPI_MASTER;
    } else {
        current_completer = CompleterID::UNKNOWN_COMPLETER;
    }
    // 1. 更新獨立 Completer 集合
    m_accessed_completer_ids_set.insert(current_completer);

    if (std::find(m_ordered_accessed_completers.begin(), m_ordered_accessed_completers.end(), current_completer) == m_ordered_accessed_completers.end()) {
        if (current_completer != CompleterID::UNKNOWN_COMPLETER ||
            (current_completer == CompleterID::UNKNOWN_COMPLETER && m_completer_transaction_counts[CompleterID::UNKNOWN_COMPLETER] == 0)) {
            m_ordered_accessed_completers.push_back(current_completer);
        }
    }

    // 3. 更新該 Completer 的交易計數
    m_completer_transaction_counts[current_completer]++;

    // 4. 確保 m_completer_bit_activity_map 中有該 completer 的條目
    if (m_completer_bit_activity_map.find(current_completer) == m_completer_bit_activity_map.end()) {
        m_completer_bit_activity_map[current_completer] = CompleterBitActivity();
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

}  // namespace APBSystem
