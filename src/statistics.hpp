#pragma once
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "apb_types.hpp"
class Statistics {
   public:
    Statistics();
    void record_transaction_completion(const TransactionData& tx_data);
    void record_pclk_cycle();
    void record_idle_cycle();
    void record_active_bus_cycle();

    void record_error_occurrence(const std::string& error_type_key);

    long long get_read_no_wait() const { return read_transactions_no_wait_; }
    long long get_read_wait() const { return read_transactions_wait_; }
    long long get_write_no_wait() const { return write_transactions_no_wait_; }
    long long get_write_wait() const { return write_transactions_wait_; }

    std::string get_avg_read_cycle_str() const;
    std::string get_avg_write_cycle_str() const;
    std::string get_bus_utilization_str() const;
    long long get_total_idle_cycles() const { return total_idle_cycles_; }
    int get_num_completers() const { return static_cast<int>(active_completers_.size()); }
    long long get_error_count(const std::string& error_type_key) const;

   private:
    long long read_transactions_no_wait_;
    long long read_transactions_wait_;
    long long write_transactions_no_wait_;
    long long write_transactions_wait_;

    long long total_pclk_cycles_;
    long long total_idle_cycles_;
    long long total_active_bus_cycles_;

    std::set<std::string> active_completers_;
    std::map<std::string, long long> error_count_map_;

    void update_completer(const TransactionData& tx_data);
};