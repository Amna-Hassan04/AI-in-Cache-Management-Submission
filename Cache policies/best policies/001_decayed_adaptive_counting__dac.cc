#include <vector>
#include <cstdint>
#include <iostream>
#include "../inc/champsim_crc2.h"

#define NUM_CORE 1
#define LLC_SETS (NUM_CORE * 2048)
#define LLC_WAYS 16

// Global usage counters for each cache line
static uint32_t usage_counters[LLC_SETS][LLC_WAYS];

void InitReplacementState() {
    // Initialize all usage counters to zero
    for (uint32_t set = 0; set < LLC_SETS; ++set) {
        for (uint32_t way = 0; way < LLC_WAYS; ++way) {
            usage_counters[set][way] = 0;
        }
    }
}

uint32_t GetVictimInSet(
    uint32_t cpu,
    uint32_t set,
    const BLOCK *current_set,
    uint64_t PC,
    uint64_t paddr,
    uint32_t type
) {
    uint32_t victim_way = 0;
    uint32_t min_counter = usage_counters[set][0]; // Start with first way's counter

    // Find the way with the minimum usage counter
    for (uint32_t way = 1; way < LLC_WAYS; ++way) {
        if (usage_counters[set][way] < min_counter) {
            min_counter = usage_counters[set][way];
            victim_way = way;
        }
    }
    return victim_way;
}

void UpdateReplacementState(
    uint32_t cpu,
    uint32_t set,
    uint32_t way,
    uint64_t paddr,
    uint64_t PC,
    uint64_t victim_addr,
    uint32_t type,
    uint8_t hit
) {
    if (hit) {
        // On cache hit, increment the usage counter
        ++usage_counters[set][way];
    } else {
        // On cache miss, initialize the counter for the new block
        usage_counters[set][way] = 1;
    }
}

void PrintStats() {
    // Optional: Print aggregated statistics if needed
}

void PrintStats_Heartbeat() {
    // Apply decay to all usage counters periodically
    for (uint32_t set = 0; set < LLC_SETS; ++set) {
        for (uint32_t way = 0; way < LLC_WAYS; ++way) {
            usage_counters[set][way] >>= 1; // Right shift to decay the counter
        }
    }
}