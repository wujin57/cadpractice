#pragma once
#include <chrono>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "apb_types.hpp"

std::string format_double_stat_detail(double val, int precision = 2);
class Statistics {
   public:
    Statistics();

    void record_transaction_completion(const APBSystem::TransactionData& tx_data);

    // Finalize CPU time measurement (call this before generating report)
    void finalize_processing();

    void record_pclk_cycle();
    void record_idle_cycle();
    void record_active_bus_cycle();

    long long get_num_read_no_wait() const { return read_transactions_no_wait_; }
    long long get_num_read_with_wait() const { return read_transactions_wait_; }
    long long get_num_write_no_wait() const { return write_transactions_no_wait_; }
    long long get_num_write_with_wait() const { return write_transactions_wait_; }

    double get_average_read_cycle() const;
    double get_average_write_cycle() const;

    double get_bus_utilization() const;
    long long get_num_idle_cycles() const;

    int get_num_completers() const;
    long long get_cpu_elapsed_time_ms() const;

    std::string get_total_pclk_cycles_str() const {
        return std::to_string(get_total_pclk_cycles_raw());
    }
    std::string get_avg_read_cycle_str() const {
        return format_double_stat_detail(get_average_read_cycle());
    }
    std::string get_avg_write_cycle_str() const {
        return format_double_stat_detail(get_average_write_cycle());
    }
    std::string get_bus_utilization_str() const {
        return format_double_stat_detail(get_bus_utilization());
    }
    std::string get_total_idle_cycles_str() const {
        return std::to_string(get_num_idle_cycles());
    }
    std::string get_num_completers_str() const {
        return std::to_string(get_num_completers());
    }
    std::string get_cpu_elapsed_time_ms_str() const {
        return std::to_string(get_cpu_elapsed_time_ms());
    }

    // Getter for transactions per completer (for report)
    std::map<APBSystem::CompleterLogicalID, long long> get_transactions_per_completer() const {
        return transactions_per_completer_;
    }

    // Other useful raw value getters for debugging or detailed reports
    long long get_total_completed_read_transactions() const {
        return completed_read_transactions_;
    }
    long long get_total_completed_write_transactions() const {
        return completed_write_transactions_;
    }
    long long get_total_pclk_cycles_raw() const {
        return total_pclk_cycles_;
    }
    long long get_total_active_bus_cycles_raw() const {
        return total_bus_active_cycles_;
    }

   private:
    // General cycle counters
    long long total_pclk_cycles_;
    long long total_idle_cycles_;
    long long total_bus_active_cycles_;

    // Read transaction counters
    long long completed_read_transactions_;
    long long sum_read_cycle_durations_;
    long long read_transactions_no_wait_;
    long long read_transactions_wait_;

    // Write transaction counters
    long long completed_write_transactions_;
    long long sum_write_cycle_durations_;
    long long write_transactions_no_wait_;
    long long write_transactions_wait_;

    // Completer
    std::map<APBSystem::CompleterLogicalID, long long> transactions_per_completer_;
    std::set<APBSystem::CompleterLogicalID> active_completers_;  // Track active completers

    // CPU Time
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
    std::chrono::time_point<std::chrono::high_resolution_clock> end_time_;
    bool processing_finalized_;
};