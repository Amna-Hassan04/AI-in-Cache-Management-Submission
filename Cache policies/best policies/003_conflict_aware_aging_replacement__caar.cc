#include <vector>
#include <cstdint>
#include <iostream>
#include "../inc/champsim_crc2.h"

#define NUM_CORE 1
#define LLC_SETS   (NUM_CORE * 2048)   // number of sets in LLC
#define LLC_WAYS   16                  // associativity

// -----------------------------------------------------------------------------
//  Replacement metadata
// -----------------------------------------------------------------------------
static uint8_t  lru_age   [LLC_SETS][LLC_WAYS] = {0};   // 0 = MRU, W-1 = LRU
static uint8_t  conflict   [LLC_SETS][LLC_WAYS] = {0};   // conflict counter

// -----------------------------------------------------------------------------
//  Initialise replacement state
// -----------------------------------------------------------------------------
void InitReplacementState() {
    // Metadata is zero‑initialised by the static definition above.
    // Nothing else to do.
}

// -----------------------------------------------------------------------------
//  Pick a victim line in a set
// -----------------------------------------------------------------------------
uint32_t GetVictimInSet(
    uint32_t cpu,
    uint32_t set,
    const BLOCK *current_set,
    uint64_t PC,
    uint64_t paddr,
    uint32_t type
) {
    // Compute replacement score for each way
    uint32_t victim = 0;
    double   max_score = -1.0;

    for (uint32_t way = 0; way < LLC_WAYS; ++way) {
        if (!current_set[way].valid) {        // empty line – immediate victim
            return way;
        }
        double cf = static_cast<double>(conflict[set][way]) /
                     (conflict[set][way] + 1.0);   // between 0 and 1
        double score = static_cast<double>(lru_age[set][way]) * (1.0 + cf);

        if (score > max_score) {
            max_score = score;
            victim = way;
        }
    }
    return victim;
}

// -----------------------------------------------------------------------------
//  Update replacement state after an access
// -----------------------------------------------------------------------------
void UpdateReplacementState(
    uint32_t cpu,
    uint32_t set,
    uint32_t way,
    uint64_t paddr,
    uint64_t PC,
    uint64_t victim_addr,
    uint32_t type,
    uint8_t  hit
) {
    // 1. Hit: bump the hit line to MRU (age=0) and shift others
    if (hit) {
        // Age of the hit way becomes 0; others increase by 1
        uint8_t old_age = lru_age[set][way];
        for (uint32_t w = 0; w < LLC_WAYS; ++w) {
            if (w == way) continue;
            if (lru_age[set][w] <= old_age)
                lru_age[set][w] += 1;
        }
        lru_age[set][way] = 0;
        return;
    }

    // 2. Miss: the victim chosen earlier is being overwritten
    //    (the caller will provide `way` as that victim)
    //    Increment its conflict counter to indicate a conflict eviction
    conflict[set][way]++;

    //    Reset the new line's age & conflict
    lru_age[set][way]   = 0;
    conflict[set][way]  = 0;
}

// -----------------------------------------------------------------------------
//  Statistics (optional)
// -----------------------------------------------------------------------------
void PrintStats() {
    // Example: print average conflict counter per set
    double total = 0;
    for (uint32_t s = 0; s < LLC_SETS; ++s)
        for (uint32_t w = 0; w < LLC_WAYS; ++w)
            total += conflict[s][w];
    std::cout << "Avg conflict per line: " << total / (LLC_SETS * LLC_WAYS) << "\n";
}

void PrintStats_Heartbeat() {
    // Could print progress or hit‑rate periodically
}