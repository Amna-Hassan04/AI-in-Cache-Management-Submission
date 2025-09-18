#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_set>
#include <iostream>
#include <random>
#include "../inc/champsim_crc2.h"

#define NUM_CORE 1
#define LLC_SETS (NUM_CORE * 2048)
#define LLC_WAYS 16

// Per-line 16-bit metadata
static std::vector<std::vector<uint16_t>> meta;

// Shadow structure to compute PC entropy
struct EntropyTracker {
    std::unordered_set<uint64_t> pcs;
    uint64_t last_update;
};
static std::vector<std::vector<EntropyTracker>> entropy;

// Global heartbeat
static uint64_t access_ctr = 0;
static std::mt19937 rng(0xCAFEBABE);   // deterministic for repeatability

// ------------------------------------------------------------------
// REQUIRED: initialize metadata
// ------------------------------------------------------------------
void InitReplacementState() {
    meta.assign(LLC_SETS, std::vector<uint16_t>(LLC_WAYS, 0));
    entropy.assign(LLC_SETS, std::vector<EntropyTracker>(LLC_WAYS));
    for (uint32_t s = 0; s < LLC_SETS; ++s)
        for (uint32_t w = 0; w < LLC_WAYS; ++w)
            entropy[s][w].last_update = 0;
}

// ------------------------------------------------------------------
// REQUIRED: choose victim
// ------------------------------------------------------------------
uint32_t GetVictimInSet(
    uint32_t cpu,
    uint32_t set,
    const BLOCK *current_set,
    uint64_t PC,
    uint64_t paddr,
    uint32_t type
) {
    uint32_t victim_way = 0;
    uint16_t min_clock = 0x7FF;                // lower 11 bits
    uint8_t  min_rrp   = 0x1F;                 // upper 5 bits
    for (uint32_t w = 0; w < LLC_WAYS; ++w) {
        uint16_t cnt = meta[set][w];
        uint16_t clock = cnt & 0x7FF;
        uint8_t  rrp   = (cnt >> 11) & 0x1F;
        if (clock < min_clock || (clock == min_clock && rrp < min_rrp)) {
            min_clock = clock;
            min_rrp   = rrp;
            victim_way = w;
        }
    }
    return victim_way;
}

// ------------------------------------------------------------------
// REQUIRED: update state
// ------------------------------------------------------------------
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
    access_ctr++;
    // sample entropy every million accesses
    if ((access_ctr & 0xFFFFF) == 0) {
        uint32_t s = rng() % LLC_SETS;
        uint32_t w = rng() % LLC_WAYS;
        entropy[s][w].pcs.insert(PC);
    }

    uint16_t &cnt = meta[set][way];
    if (hit) {
        cnt++;
    } else {
        // new line: bootstrap with mid-range value
        cnt = 0x4000;
    }

    // on saturation reseed based on PC entropy
    if (cnt == 0xFFFF) {
        uint32_t e = static_cast<uint32_t>(entropy[set][way].pcs.size());
        // upper 5 bits = RRP, lower 11 bits = decay-clock
        cnt = static_cast<uint16_t>(
                (std::min(e, 31u) << 11) |
                (rng() & 0x7FF));
    }
}

// ------------------------------------------------------------------
// OPTIONAL: stats
// ------------------------------------------------------------------
void PrintStats() {
    std::cout << "ChronoEntropy: access_ctr = " << access_ctr << "\n";
}

void PrintStats_Heartbeat() {
    // silent
}