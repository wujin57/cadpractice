#include "statistics.hpp"
#include <cmath>

Statistics::Statistics() {}

void Statistics::record_pclk_cycle() {
    total_pclk_cycles++;
}

void Statistics::record_idle_cycle() {
    total_idle_cycles++;
}
void Statistics::record_active_bus_cycle() {
    total_bus_active_cycles++;
}

void Statistics::record_transaction_completion(const TransactionData& tx_data) {
    if (tx_data.completer_id != -1) {
        active_completers_.insert(tx_data.completer_id);
    }

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

std::string format_double(double val, int precision = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << val;
    return oss.str();
}

std::string Statistics::get_avg_read_cycle_str() const {
    if (completed_read_transactions_ == 0)
        return "0.00";
    double avg = static_cast<double>(sum_read_cycle_durations_) / completed_read_transactions_;
    return format_double(avg);
}

std::string Statistics::get_avg_write_cycle_str() const {
    if (completed_write_transactions_ == 0)
        return "0.00";
    double avg = static_cast<double>(sum_write_cycle_durations_) / completed_write_transactions_;
    return format_double(avg);
}

std::string Statistics::get_bus_utilization_str() const {
    if (total_pclk_cycles_ == 0)
        return "0.00";
    // Bus utilization defined as (total transaction cycles / total bus cycles) * 100
    // total transaction cycles = sum of cycles for all valid read and write transactions
    // total_bus_active_cycles_ should represent this (sum of duration_pclk_cycles for all tx)
    double util = (static_cast<double>(total_bus_active_cycles_) / total_pclk_cycles_) * 100.0;
    return format_double(util);
}