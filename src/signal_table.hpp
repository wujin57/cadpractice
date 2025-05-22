//
// Created by wujin on 2025/5/1.
//

#pragma once
#include <string>
#include "transaction.hpp"  // Added this include as SignalState is used

// Global SignalState is problematic if this is a generic signal table.
// Consider passing SignalState& to handle_signal_event.
// extern SignalState signal_state; // From transaction.hpp/cpp
// extern int current_time;      // From transaction.hpp/cpp

void register_signal(char** tokens, int count);
// handle_signal_event should update the global signal_state and current_time
// based on the VCD line.
void handle_signal_event(const char* line);