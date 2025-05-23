// error_logger.cpp
#include "error_logger.hpp"

void ErrorLogger::log_error(int timestamp, const std::string& formatted_message) {
    errors_.push_back({timestamp, formatted_message});
    sorted_ = false;  // New error, list is no longer guaranteed to be sorted
}

void ErrorLogger::sort_errors() {
    if (!sorted_) {
        std::sort(errors_.begin(), errors_.end());
        sorted_ = true;
    }
}

const std::vector<ErrorInfo>& ErrorLogger::get_errors() const {
    // Optionally, always sort before returning if strict ordering is always needed upon get
    // sort_errors();
    return errors_;
}