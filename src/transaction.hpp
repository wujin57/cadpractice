#pragma once

#include <algorithm>  // For std::min, std::max
#include <cstdint>    // For uint32_t
#include <string>
#include <vector>

// Forward declaration of SignalState if its full definition is in transaction.cpp
// or if it's complex. For simplicity here, assume it's knowable.
struct SignalState {
    uint32_t paddr = 0;
    uint32_t pwdata = 0;
    uint32_t prdata = 0;
    bool pwrite = false;
    bool psel = false;  // Assuming one PSEL, or this will be part of a more complex completer ID logic
    // Add other PSELs if they exist, e.g., bool psel_slave0, psel_slave1...
    bool penable = false;
    bool pready = false;
    bool pslverr = false;
    bool presetn = true;  // Active low reset
    bool pclk = false;    // APB clock
    // Add other signals as needed from VCD
};

// --- Structure for Identified Faulty Bit Pairs ---
struct IdentifiedFloatingPair {
    int bit1 = -1;
    int bit2 = -1;
    bool isActive = false;
    char signal_char = ' ';  // 'a' for PADDR, 'd' for PWDATA

    IdentifiedFloatingPair(int b1 = -1, int b2 = -1, bool active = false, char sc = ' ') : bit1(std::min(b1, b2)), bit2(std::max(b1, b2)), isActive(active), signal_char(sc) {}
    IdentifiedFloatingPair() : bit1(-1), bit2(-1), isActive(false), signal_char(' ') {}  // Default constructor
};

// --- Global State (defined in transaction.cpp) ---
extern SignalState signal_state;  // Current state of APB signals
extern SignalState prev_state;    // Previous state of APB signals (for edge detection or comparison)
extern int current_time;          // Current VCD timestamp, updated by parser

// --- Constants ---
constexpr int MAX_COMPLETERS = 5;    // Max number of completers as per contest PDF
constexpr int MAX_SIGNAL_BITS = 32;  // Max bit width for PADDR/PWDATA for stats arrays

// --- Statistics for discovering faulty pairs (per completer) ---
struct BitPairComparisonStats {
    long long equal_count = 0;
    long long diff_count = 0;
};
// These will be defined in transaction.cpp
extern BitPairComparisonStats
    g_paddr_bit_stats[MAX_COMPLETERS][MAX_SIGNAL_BITS][MAX_SIGNAL_BITS];
extern BitPairComparisonStats
    g_pwdata_bit_stats[MAX_COMPLETERS][MAX_SIGNAL_BITS][MAX_SIGNAL_BITS];

// Storage for discovered faulty pairs (per completer)
extern IdentifiedFloatingPair g_identified_paddr_faults[MAX_COMPLETERS];
extern IdentifiedFloatingPair g_identified_pwdata_faults[MAX_COMPLETERS];

// --- Function Declarations ---

// Initializes structures used for fault discovery. Call once at the beginning.
void initialize_fault_discovery_structures();

// Updates statistics for bit pair comparisons.
// Call this for each relevant PADDR/PWDATA value during VCD parsing,
// ensuring completer_id is correctly determined.
// num_bits should be the actual width of paddr_val or pwdata_val.
void update_paddr_fault_discovery_stats(int completer_id, uint32_t paddr_val, int num_bits);
void update_pwdata_fault_discovery_stats(int completer_id, uint32_t pwdata_val, int num_bits);

// Analyzes collected statistics to identify permanent 2-bit floating faults.
// Call this after parsing a significant portion or all of the VCD.
// num_paddr_bits and num_pwdata_bits are the actual widths of these buses.
void identify_fixed_faulty_pairs_for_all_completers(int num_paddr_bits, int num_pwdata_bits);

// Main function to check for APB transaction events and errors based on current signal_state.
// Assumes current_time and signal_state are correctly set by the VCD parser before each call.
void check_transaction_event();

// Retrieves the collected error log.
std::vector<std::string>& get_error_log();

// Placeholder: User needs to implement this.
// Determines which completer (0 to MAX_COMPLETERS-1) is targeted by the current transaction.
// Returns -1 if no specific completer can be identified.
// This is CRUCIAL and depends on how PSEL signals are named in VCD or address map.
int get_target_completer_id_from_transaction(const SignalState& current_signals);

// TODO: Declare functions for calculating and retrieving final contest output statistics:
// - Number of Read Transactions (no wait, with wait)
// - Number of Write Transactions (no wait, with wait)
// - Number of Idle Cycles
// - Average Read Cycle
// - Average Write Cycle
// - Bus Utilization (%)
// - CPU Elapsed Time (ms) - This is measured by the evaluator usually.
// - Lists of other error types (Timeout, Out-of-Range, Mirroring, Read-Write Overlap)
