#pragma once
#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

struct ErrorInfo {
    int timestamp;
    std::string message;

    bool operator<(const ErrorInfo& other) const {
        if (timestamp != other.timestamp) {
            return timestamp < other.timestamp;
        }
        // Optional: secondary sort by message if timestamps are equal
        return message < other.message;
    }
};

class ErrorLogger {
   public:
    void log_error(int timestamp, const std::string& formatted_message);

    // Call this before getting errors if they might not be sorted
    void sort_errors();
    const std::vector<ErrorInfo>& get_errors() const;

   private:
    std::vector<ErrorInfo> errors_;
    bool sorted_ = false;
};