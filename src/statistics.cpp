#include "statistics.hpp"
#include <cmath>
std::string format_double_stat_detail(double val, int precision = 2) {  // 重新命名以避免衝突
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << val;
    return oss.str();
}

Statistics::Statistics() {}

void Statistics::record_pclk_cycle() {
    total_pclk_cycles_++;
}

void Statistics::record_idle_cycle() {
    total_idle_cycles_++;
}
void Statistics::record_active_bus_cycle() {
    total_bus_active_cycles_++;
}

void Statistics::record_completer_activity(int completer_id) {
    // 使用 APBSystem 命名空間中的常數
    if (completer_id >= 0 && completer_id < APBSystem::MAX_COMPLETERS) {
        active_completers_.insert(completer_id);
    }
}

// 參數類型使用 APBSystem::TransactionData
void Statistics::record_transaction_completion(const APBSystem::TransactionData& tx_data) {  // <<<< 修正
    record_completer_activity(tx_data.completer_id);

    if (tx_data.is_write) {
        completed_write_transactions_++;
        sum_write_cycle_durations_ += tx_data.duration_pclk_cycles;
        if (tx_data.has_wait_states) {
            write_transactions_wait_++;
        } else {
            write_transactions_no_wait_++;
        }
    } else {
        completed_read_transactions_++;
        sum_read_cycle_durations_ += tx_data.duration_pclk_cycles;
        if (tx_data.has_wait_states) {
            read_transactions_wait_++;
        } else {
            read_transactions_no_wait_++;
        }
    }
}

void Statistics::record_error_occurrence(const std::string& error_type_key) {
    error_counts_[error_type_key]++;
}

long long Statistics::get_error_count(const std::string& error_type_key) const {
    auto it = error_counts_.find(error_type_key);
    return (it != error_counts_.end()) ? it->second : 0;
}

std::string Statistics::get_avg_read_cycle_str() const {
    if (completed_read_transactions_ == 0)
        return "0.00";
    double avg = static_cast<double>(sum_read_cycle_durations_) / completed_read_transactions_;
    return format_double_stat_detail(avg);
}

std::string Statistics::get_avg_write_cycle_str() const {
    if (completed_write_transactions_ == 0)
        return "0.00";
    double avg = static_cast<double>(sum_write_cycle_durations_) / completed_write_transactions_;
    return format_double_stat_detail(avg);
}

std::string Statistics::get_bus_utilization_str() const {
    if (total_pclk_cycles_ == 0)
        return "0.00";
    double util = (static_cast<double>(total_bus_active_cycles_) / total_pclk_cycles_) * 100.0;
    return format_double_stat_detail(util);
}

int Statistics::get_num_completers() const {
    return static_cast<int>(active_completers_.size());
}