#include "statistics.hpp"
#include <cmath>
#include <numeric>

std::string format_double_stat_detail(double val, int precision = 2) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << val;
    return oss.str();
}

Statistics::Statistics()
    : total_pclk_cycles_(0),
      total_idle_cycles_(0),
      total_bus_active_cycles_(0),
      completed_read_transactions_(0),
      sum_read_cycle_durations_(0),
      read_transactions_no_wait_(0),
      read_transactions_wait_(0),
      completed_write_transactions_(0),
      sum_write_cycle_durations_(0),
      write_transactions_no_wait_(0),
      write_transactions_wait_(0),
      processing_finalized_(false) {
    start_time_ = std::chrono::high_resolution_clock::now();

    transactions_per_completer_[APBSystem::CompleterLogicalID::UART] = 0;
    transactions_per_completer_[APBSystem::CompleterLogicalID::GPIO] = 0;
    transactions_per_completer_[APBSystem::CompleterLogicalID::SPI_MASTER] = 0;
    transactions_per_completer_[APBSystem::CompleterLogicalID::UNKNOWN] = 0;
}

void Statistics::record_pclk_cycle() {
    total_pclk_cycles_++;
}

void Statistics::record_idle_cycle() {
    total_idle_cycles_++;
}
void Statistics::record_active_bus_cycle() {
    total_bus_active_cycles_++;
}

void Statistics::record_transaction_completion(const APBSystem::TransactionData& tx_data) {
    if (tx_data.completer_id_ != APBSystem::CompleterLogicalID::UNKNOWN) {
        active_completers_.insert(tx_data.completer_id_);
        transactions_per_completer_[tx_data.completer_id_]++;
    }
    if (tx_data.is_write_) {
        completed_write_transactions_++;
        sum_write_cycle_durations_ += tx_data.duration_pclk_cycles_;
        if (tx_data.has_wait_states_) {
            write_transactions_wait_++;
        } else {
            write_transactions_no_wait_++;
        }
    } else {  // read
        completed_read_transactions_++;
        sum_read_cycle_durations_ += tx_data.duration_pclk_cycles_;
        if (tx_data.has_wait_states_) {
            read_transactions_wait_++;
        } else {
            read_transactions_no_wait_++;
        }
    }
}

void Statistics::finalize_processing() {
    if (!processing_finalized_) {
        end_time_ = std::chrono::high_resolution_clock::now();
        processing_finalized_ = true;
    }
}

double Statistics::get_average_read_cycle() const {
    if (completed_read_transactions_ == 0)
        return 0.0;
    return static_cast<double>(sum_read_cycle_durations_) / completed_read_transactions_;
}
double Statistics::get_average_write_cycle() const {
    if (completed_write_transactions_ == 0)
        return 0.0;
    return static_cast<double>(sum_write_cycle_durations_) / completed_write_transactions_;
}
double Statistics::get_bus_utilization() const {
    if (total_pclk_cycles_ == 0)
        return 0.0;
    return static_cast<double>(total_bus_active_cycles_) / total_pclk_cycles_;
}
long long Statistics::get_num_idle_cycles() const {
    return total_idle_cycles_;
}
int Statistics::get_num_completers() const {
    return static_cast<int>(active_completers_.size());
}

long long Statistics::get_cpu_elapsed_time_ms() const {
    if (!processing_finalized_) {
        auto temp_end_time = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(temp_end_time - start_time_).count();
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_).count();
}
