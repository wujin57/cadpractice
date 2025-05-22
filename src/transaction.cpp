#include "transaction.hpp"  // Should include the definition of SignalState if not here
#include <algorithm>        // For std::min, std::max
#include <bitset>
#include <iomanip>
#include <iostream>
#include <map>  // Could be useful for completer-specific data if needed
#include <sstream>
#include <string>
#include <vector>

// --- Global Definitions ---
SignalState signal_state;
SignalState prev_state;
int current_time = 0;

std::vector<std::string> g_error_log_vector;

// Statistics for discovering faulty pairs
BitPairComparisonStats
    g_paddr_bit_stats[MAX_COMPLETERS][MAX_SIGNAL_BITS][MAX_SIGNAL_BITS];
BitPairComparisonStats
    g_pwdata_bit_stats[MAX_COMPLETERS][MAX_SIGNAL_BITS][MAX_SIGNAL_BITS];

// Storage for discovered faulty pairs
IdentifiedFloatingPair g_identified_paddr_faults[MAX_COMPLETERS];
IdentifiedFloatingPair g_identified_pwdata_faults[MAX_COMPLETERS];

// --- Transaction State (static to this file, internal to the transaction module) ---
static bool s_in_transaction = false;
static bool s_paddr_locked = false;   // True if PADDR has been latched for the current transaction
static bool s_pwdata_locked = false;  // True if PWDATA has been latched (for writes)

static uint32_t s_latched_paddr = 0;   // PADDR value latched at the start of SETUP phase
static uint32_t s_latched_pwdata = 0;  // PWDATA value latched for a write transaction

static int s_transaction_start_time = -1;  // VCD timestamp when the current transaction (SETUP) began
static int s_access_phase_pclk_count = 0;  // Number of PCLK cycles spent in the current ACCESS phase (for timeout)

// --- Configurable constants (adjust based on contest problem or APB specifics) ---
constexpr int MAX_TIMEOUT_PCLK_CYCLES = 100;  // Example: From contest PDF for timeout error
// Define PCLK_PERIOD if timeout is in absolute time, or ensure MAX_TIMEOUT_PCLK_CYCLES is in clock ticks.
// For now, assuming check_transaction_event is called effectively per PCLK cycle relevant to APB state.

// --- Helper Functions ---
// (countSetBits could be useful for other error types if needed)
// int countSetBits(uint32_t n) { ... } // Already provided previously

// --- Fault Discovery and Management Implementation ---
void initialize_fault_discovery_structures() {
    for (int c = 0; c < MAX_COMPLETERS; ++c) {
        for (int i = 0; i < MAX_SIGNAL_BITS; ++i) {
            for (int j = 0; j < MAX_SIGNAL_BITS; ++j) {  // No need to clear j < i, as we iterate j > i later
                g_paddr_bit_stats[c][i][j] = {0, 0};
                g_pwdata_bit_stats[c][i][j] = {0, 0};
            }
        }
        // Initialize IdentifiedFloatingPair with default constructor (isActive=false)
        g_identified_paddr_faults[c] = IdentifiedFloatingPair();
        g_identified_pwdata_faults[c] = IdentifiedFloatingPair();
    }
}

// User MUST implement this robustly based on VCD signal naming (e.g., PSEL_SLAVE0)
// or documented PADDR ranges for each completer. This is a placeholder.
int get_target_completer_id_from_transaction(const SignalState& current_signals) {
    // --- BEGIN CRITICAL USER IMPLEMENTATION ---
    // Example: If VCD has signals like 'TOP.APB_SUBSYSTEM.SLAVE0.PSEL'
    // and your signal registration maps these to boolean flags in SignalState:
    // if (current_signals.specific_psel_for_completer_0) return 0;
    // if (current_signals.specific_psel_for_completer_1) return 1;
    // ...
    // Or, if based on PADDR ranges (these ranges must be known):
    // if (current_signals.paddr >= COMPLETER0_ADDR_START && current_signals.paddr <= COMPLETER0_ADDR_END) return 0;
    // if (current_signals.paddr >= COMPLETER1_ADDR_START && current_signals.paddr <= COMPLETER1_ADDR_END) return 1;

    // Fallback for single PSEL scenario (assuming only one possible completer or default to completer 0)
    if (current_signals.psel) {  // If there's a general PSEL active
        return 0;                // Defaulting to completer 0; THIS NEEDS TO BE ACCURATE
    }
    // --- END CRITICAL USER IMPLEMENTATION ---
    return -1;  // No completer identified / PSEL not active for this check
}

void update_paddr_fault_discovery_stats(int completer_id, uint32_t paddr_val, int num_bits) {
    if (completer_id < 0 || completer_id >= MAX_COMPLETERS || num_bits <= 1 || num_bits > MAX_SIGNAL_BITS) {
        return;
    }
    for (int i = 0; i < num_bits; ++i) {
        for (int j = i + 1; j < num_bits; ++j) {  // Iterate j > i
            bool bit_i_val = (paddr_val >> i) & 1;
            bool bit_j_val = (paddr_val >> j) & 1;
            if (bit_i_val == bit_j_val) {
                g_paddr_bit_stats[completer_id][i][j].equal_count++;
            } else {
                g_paddr_bit_stats[completer_id][i][j].diff_count++;
            }
        }
    }
}

void update_pwdata_fault_discovery_stats(int completer_id, uint32_t pwdata_val, int num_bits) {
    if (completer_id < 0 || completer_id >= MAX_COMPLETERS || num_bits <= 1 || num_bits > MAX_SIGNAL_BITS) {
        return;
    }
    for (int i = 0; i < num_bits; ++i) {
        for (int j = i + 1; j < num_bits; ++j) {  // Iterate j > i
            bool bit_i_val = (pwdata_val >> i) & 1;
            bool bit_j_val = (pwdata_val >> j) & 1;
            if (bit_i_val == bit_j_val) {
                g_pwdata_bit_stats[completer_id][i][j].equal_count++;
            } else {
                g_pwdata_bit_stats[completer_id][i][j].diff_count++;
            }
        }
    }
}

void identify_fixed_faulty_pairs_for_all_completers(int num_paddr_bits, int num_pwdata_bits) {
    // Heuristic: A certain number of observations where bits were always equal, and never different.
    const long long MIN_OBSERVATIONS_FOR_CONSISTENT_EQUALITY = 10;  // Adjust as needed

    for (int c = 0; c < MAX_COMPLETERS; ++c) {
        // Identify PADDR fault for completer c
        bool paddr_fault_found_for_completer = false;
        for (int i = 0; i < num_paddr_bits && !paddr_fault_found_for_completer; ++i) {
            for (int j = i + 1; j < num_paddr_bits && !paddr_fault_found_for_completer; ++j) {
                if (g_paddr_bit_stats[c][i][j].diff_count == 0 &&
                    g_paddr_bit_stats[c][i][j].equal_count >= MIN_OBSERVATIONS_FOR_CONSISTENT_EQUALITY) {
                    g_identified_paddr_faults[c] = IdentifiedFloatingPair(i, j, true, 'a');
                    // std::cout << "INFO: Completer " << c << " PADDR fault identified: a"
                    //           << std::max(i,j) << "-a" << std::min(i,j) << " Floating." << std::endl;
                    paddr_fault_found_for_completer = true;  // Assume only one such 2-bit pair per signal per completer
                }
            }
        }

        // Identify PWDATA fault for completer c
        bool pwdata_fault_found_for_completer = false;
        for (int i = 0; i < num_pwdata_bits && !pwdata_fault_found_for_completer; ++i) {
            for (int j = i + 1; j < num_pwdata_bits && !pwdata_fault_found_for_completer; ++j) {
                if (g_pwdata_bit_stats[c][i][j].diff_count == 0 &&
                    g_pwdata_bit_stats[c][i][j].equal_count >= MIN_OBSERVATIONS_FOR_CONSISTENT_EQUALITY) {
                    g_identified_pwdata_faults[c] = IdentifiedFloatingPair(i, j, true, 'd');
                    // std::cout << "INFO: Completer " << c << " PWDATA fault identified: d"
                    //           << std::max(i,j) << "-d" << std::min(i,j) << " Floating." << std::endl;
                    pwdata_fault_found_for_completer = true;  // Assume only one
                }
            }
        }
    }
}

// --- Address/Data Corruption Logging (Permanent Fault Interpretation) ---
void log_corruption_if_applicable(uint32_t received_val,  // Value from VCD
                                  const IdentifiedFloatingPair& known_faulty_pair,
                                  const std::string& signal_name,  // "PADDR" or "PWDATA"
                                  int timestamp) {
    if (!known_faulty_pair.isActive) {
        return;
    }

    int b_small = known_faulty_pair.bit1;  // Smaller bit index of the pair
    int b_large = known_faulty_pair.bit2;  // Larger bit index of the pair
    char sig_char = known_faulty_pair.signal_char;

    bool val_b_small_rec = (received_val >> b_small) & 1;
    bool val_b_large_rec = (received_val >> b_large) & 1;

    // Due to permanent fault, these two bits should always be identical in the received_val.
    if (val_b_small_rec != val_b_large_rec) {
        // This indicates an issue with fault identification or VCD noise, as Q7 implies they are logically synchronized.
        // std::cerr << "[#" << timestamp << "] WARNING: Identified floating pair "
        //           << sig_char << b_large << "-" << sig_char << b_small
        //           << " has differing values ( " << val_b_large_rec << ", " << val_b_small_rec
        //           << ") in " << signal_name << " 0x" << std::hex << received_val << std::dec
        //           << ". This contradicts permanent fault assumption." << std::endl;
        return;
    }

    uint32_t inferred_expected_val = received_val;
    bool corruption_to_log = false;

    // The contest example: Expected PADDR: 0x08, Received: 0x0C (a5-a4 Floating)
    // PADDR 0x0C (bits ...1100). If a5(bit3), a4(bit2). Pair is (1,1).
    // PADDR 0x08 (bits ...1000). Pair was (1,0).
    // This implies a fault model: if intended pair was (1,0) or (0,1), actual is (1,1).
    // If intended (0,0) -> actual (0,0). If intended (1,1) -> actual (1,1).
    // So, corruption is logged if received pair is (1,1) AND it could have come from an expected (1,0) or (0,1).

    if (val_b_small_rec == 1 /* means pair is (1,1) */) {
        // Try to infer an "Expected" where one bit was 0.
        // Let's try to form an Expected by setting the bit with the smaller index (b_small) to 0.
        // This would make the pair in Expected (Val_b_large, 0).
        // Example: Received pair (1,1). Expected could be (1,0) by setting b_small to 0.
        inferred_expected_val = received_val ^ (1U << b_small);  // Flip b_small from 1 to 0

        // Only log if this inferred_expected_val is different from received_val
        // (it will be if b_small was flipped from 1 to 0).
        // And the original bits in this inferred_expected_val for the pair (b_large, b_small) are now different.
        bool expected_b_small_val = (inferred_expected_val >> b_small) & 1;  // Should be 0
        bool expected_b_large_val = (inferred_expected_val >> b_large) & 1;  // Should be 1

        if (expected_b_small_val != expected_b_large_val) {  // Confirms the inferred Expected has different bits for the pair
            corruption_to_log = true;
        } else {
            // This case should not happen if we correctly formed inferred_expected_val by flipping one bit of a (1,1) pair.
            // It means our inference for "Expected" is flawed or the situation is more complex.
        }
    }
    // If received pair is (0,0), we assume Expected was also (0,0) under this model.
    // No "Expected vs Received" corruption is logged as values are consistent with a (0,0) input to the fault.

    if (corruption_to_log) {
        std::ostringstream oss_err_detail;
        oss_err_detail << "(" << sig_char << b_large << "-" << sig_char << b_small << " Floating)";

        std::ostringstream oss_err;
        oss_err << "[#" << timestamp << "] " << (signal_name == "PADDR" ? "Address" : "Data")
                << " Corruption -> Expected " << signal_name << ": 0x" << std::hex << inferred_expected_val
                << ", Received: 0x" << received_val << " " << oss_err_detail.str();
        g_error_log_vector.push_back(oss_err.str());
        // std::cout << oss_err.str() << std::endl;
    }
}

// --- Transaction Lifecycle (internal helpers) ---
void begin_transaction_internal() {
    s_latched_paddr = signal_state.paddr;  // PADDR on the bus (already reflects permanent fault if any)
    s_paddr_locked = true;

    if (signal_state.pwrite) {
        s_latched_pwdata = signal_state.pwdata;  // PWDATA on the bus
        s_pwdata_locked = true;
    } else {
        s_pwdata_locked = false;
    }
    s_in_transaction = true;
    s_transaction_start_time = current_time;
    s_access_phase_pclk_count = 0;

    // Debug output (optional)
    // std::cout << "[Time: " << current_time << "] Transaction Begin (SETUP): PADDR=0x" << std::hex << s_latched_paddr
    //           << (signal_state.pwrite ? ", PWDATA=0x" + std::to_string(s_latched_pwdata) : "") << std::dec << std::endl;
}

void end_transaction_internal(bool is_error = false, const std::string& reason = "") {
    // Debug output or statistics update can happen here
    // if (!reason.empty()) {
    //     std::cout << "[Time: " << current_time << "] Transaction Ended. Reason: " << reason << std::endl;
    // } else {
    //     std::cout << "[Time: " << current_time << "] Transaction Ended." << std::endl;
    // }

    s_paddr_locked = false;
    s_pwdata_locked = false;
    s_in_transaction = false;
    s_transaction_start_time = -1;
    s_access_phase_pclk_count = 0;  // Reset for next transaction
}

// --- Main Event Checker ---
void check_transaction_event() {
    static enum { FSM_IDLE,
                  FSM_SETUP,
                  FSM_ACCESS } apb_fsm_state = FSM_IDLE;

    if (!signal_state.presetn) {  // Active low reset
        if (s_in_transaction) {
            // Log or count aborted transaction due to reset
            std::ostringstream oss_err;
            oss_err << "[#" << current_time << "] Transaction Aborted -> PRESETn active during transaction at PADDR 0x"
                    << std::hex << s_latched_paddr;
            g_error_log_vector.push_back(oss_err.str());
            end_transaction_internal(true, "PRESETn asserted");
        }
        apb_fsm_state = FSM_IDLE;
        prev_state = signal_state;
        return;
    }

    // --- Update Fault Discovery Statistics ---
    // This should be done for relevant signals when PSEL indicates an active completer.
    // The actual bit widths for PADDR and PWDATA must be known (e.g., from $var declarations).
    // Assuming paddr_width and pwdata_width are known variables.
    int paddr_width = 32;   // Placeholder, determine from VCD
    int pwdata_width = 32;  // Placeholder, determine from VCD

    int current_completer_id = -1;
    if (signal_state.psel) {  // Only if a slave is selected general PSEL is high
        current_completer_id = get_target_completer_id_from_transaction(signal_state);
        if (current_completer_id != -1) {
            // PADDR is generally considered valid in SETUP and ACCESS phases when PSEL is high
            update_paddr_fault_discovery_stats(current_completer_id, signal_state.paddr, paddr_width);

            // PWDATA for writes is typically driven/valid during ACCESS phase
            if (apb_fsm_state == FSM_ACCESS && signal_state.pwrite) {
                update_pwdata_fault_discovery_stats(current_completer_id, signal_state.pwdata, pwdata_width);
            }
        }
    }

    // --- PADDR/PWDATA Instability Checks (value changing when it should be stable) ---
    // These are checked if a transaction is ongoing and in ACCESS phase.
    if (apb_fsm_state == FSM_ACCESS) {
        s_access_phase_pclk_count++;

        if (s_paddr_locked && signal_state.paddr != s_latched_paddr) {
            std::ostringstream oss_err;
            oss_err << "[#" << current_time << "] PADDR Instability Error -> Latched at SETUP: 0x"
                    << std::hex << s_latched_paddr << ", Changed to: 0x" << signal_state.paddr
                    << " during ACCESS phase.";
            g_error_log_vector.push_back(oss_err.str());
            // s_latched_paddr = signal_state.paddr; // Optionally update to new unstable value for further checks
        }
        if (signal_state.pwrite && s_pwdata_locked && signal_state.pwdata != s_latched_pwdata) {
            // PWDATA is latched at SETUP (if pwrite) or start of ACCESS. If it changes during ACCESS before PREADY, it's unstable.
            std::ostringstream oss_err;
            oss_err << "[#" << current_time << "] PWDATA Instability Error -> Latched: 0x"
                    << std::hex << s_latched_pwdata << ", Changed to: 0x" << signal_state.pwdata
                    << " during ACCESS phase.";
            g_error_log_vector.push_back(oss_err.str());
            // s_latched_pwdata = signal_state.pwdata; // Optionally update
        }
    }

    // --- APB State Machine Logic ---
    switch (apb_fsm_state) {
        case FSM_IDLE:
            if (signal_state.psel && !signal_state.penable) {  // Start of SETUP phase
                begin_transaction_internal();
                apb_fsm_state = FSM_SETUP;
            }
            break;

        case FSM_SETUP:
            // PADDR should remain stable throughout SETUP. If it changed from the initial latch:
            if (s_paddr_locked && signal_state.paddr != s_latched_paddr) {
                // This is distinct from the permanent fault corruption. This is instability.
                std::ostringstream oss_err;
                oss_err << "[#" << current_time << "] PADDR Instability Error -> Latched at transaction start: 0x"
                        << std::hex << s_latched_paddr << ", Changed to: 0x" << signal_state.paddr
                        << " during SETUP phase.";
                g_error_log_vector.push_back(oss_err.str());
            }
            // Similarly for PWDATA if latched at SETUP for writes

            if (signal_state.psel && signal_state.penable) {  // Transition to ACCESS phase
                apb_fsm_state = FSM_ACCESS;
                s_access_phase_pclk_count = 1;  // First cycle in ACCESS

                // --- Log "Address/Data Corruption" due to Permanent Fault ---
                // This check is done ONCE at the transition to ACCESS phase, using the
                // PADDR/PWDATA values that are now established for the ACCESS phase.
                // These values already reflect any permanent fault.
                if (current_completer_id != -1) {  // current_completer_id from stats update section
                    log_corruption_if_applicable(signal_state.paddr,
                                                 g_identified_paddr_faults[current_completer_id],
                                                 "PADDR", current_time);
                    if (signal_state.pwrite) {
                        log_corruption_if_applicable(signal_state.pwdata,
                                                     g_identified_pwdata_faults[current_completer_id],
                                                     "PWDATA", current_time);
                    }
                }

            } else if (!signal_state.psel && s_in_transaction) {  // PSEL dropped during SETUP
                std::ostringstream oss_err;
                oss_err << "[#" << current_time << "] Protocol Error -> PSEL dropped during SETUP phase for PADDR 0x"
                        << std::hex << s_latched_paddr;
                g_error_log_vector.push_back(oss_err.str());
                end_transaction_internal(true, "PSEL dropped during SETUP");
                apb_fsm_state = FSM_IDLE;
            }
            break;

        case FSM_ACCESS:
            if (signal_state.pready) {       // Transaction ends
                if (signal_state.pslverr) {  // Slave error
                    std::ostringstream oss_err;
                    oss_err << "[#" << current_time << "] PSLVERR Error -> Transaction failed with PSLVERR at PADDR 0x"
                            << std::hex << s_latched_paddr;
                    g_error_log_vector.push_back(oss_err.str());
                    end_transaction_internal(true, "PSLVERR asserted");
                } else {  // Normal completion
                    end_transaction_internal(false);
                }
                apb_fsm_state = FSM_IDLE;
            } else if (!signal_state.psel || !signal_state.penable) {  // PSEL or PENABLE dropped before PREADY
                if (s_in_transaction) {
                    std::ostringstream oss_err;
                    oss_err << "[#" << current_time << "] Protocol Error -> PSEL/PENABLE dropped during ACCESS before PREADY for PADDR 0x"
                            << std::hex << s_latched_paddr;
                    g_error_log_vector.push_back(oss_err.str());
                    end_transaction_internal(true, "PSEL/PENABLE dropped mid-ACCESS");
                }
                apb_fsm_state = FSM_IDLE;
            } else {  // Still in ACCESS, PREADY is low, check for timeout
                if (s_access_phase_pclk_count > MAX_TIMEOUT_PCLK_CYCLES) {
                    std::ostringstream oss_err;
                    oss_err << "[#" << current_time << "] Timeout Error -> Transaction at PADDR 0x"
                            << std::hex << s_latched_paddr << " exceeded " << MAX_TIMEOUT_PCLK_CYCLES
                            << " PCLK cycles in ACCESS phase.";
                    g_error_log_vector.push_back(oss_err.str());
                    end_transaction_internal(true, "Transaction Timeout");
                    apb_fsm_state = FSM_IDLE;
                }
            }
            break;
    }
    prev_state = signal_state;  // Store current state for the next cycle's prev_state
}

std::vector<std::string>& get_error_log() {
    return g_error_log_vector;
}