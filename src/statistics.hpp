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
    // 使用命名空間指定 TransactionData
    void record_transaction_completion(const APBSystem::TransactionData& tx_data);  // <<<< 修正
    void record_pclk_cycle();
    void record_idle_cycle();
    void record_active_bus_cycle();
    void record_error_occurrence(const std::string& error_type_key);
    void record_completer_activity(int completer_id);

    long long get_read_no_wait() const { return read_transactions_no_wait_; }
    long long get_read_wait() const { return read_transactions_wait_; }
    long long get_write_no_wait() const { return write_transactions_no_wait_; }
    long long get_write_wait() const { return write_transactions_wait_; }
    std::string get_avg_read_cycle_str() const;
    std::string get_avg_write_cycle_str() const;
    std::string get_bus_utilization_str() const;
    long long get_total_idle_cycles() const { return total_idle_cycles_; }
    int get_num_completers() const;
    long long get_error_count(const std::string& error_type_key) const;

    // 如果您的 statistics.hpp 第44行確實有 update_completer，也需要修正：
    // void update_completer(const APBSystem::TransactionData& tx_data); // <<<< 範例修正

   private:
    long long read_transactions_no_wait_ = 0;
    long long read_transactions_wait_ = 0;
    long long write_transactions_no_wait_ = 0;
    long long write_transactions_wait_ = 0;

    long long sum_read_cycle_durations_ = 0;
    long long completed_read_transactions_ = 0;
    long long sum_write_cycle_durations_ = 0;
    long long completed_write_transactions_ = 0;

    long long total_pclk_cycles_ = 0;
    long long total_idle_cycles_ = 0;
    long long total_bus_active_cycles_ = 0;

    std::set<int> active_completers_;
    std::map<std::string, long long> error_counts_;
};