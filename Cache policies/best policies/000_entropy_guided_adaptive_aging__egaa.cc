/*******************************************************************
 * Entropy‑Guided Adaptive Aging (EGAA) replacement policy
 *
 *  - per‑line 8‑bit age counter
 *  - per‑line 8‑bit entropy accumulator
 *  - per‑line last observed address (64‑bit)
 *  - per‑line last observed PC     (64‑bit)
 *
 *  Author: <your name>
 *******************************************************************/

#include <vector>
#include <cstdint>
#include <iostream>
#include "../inc/champsim_crc2.h"

#define NUM_CORE 1
#define LLC_SETS (NUM_CORE * 2048)   // total number of sets
#define LLC_WAYS 16                  // associativity

/*-----------------------------------------------------------------
 *  Policy‑specific metadata
 *-----------------------------------------------------------------*/
struct EGAA_LineInfo {
    uint8_t  age;        // aging counter (0‑255)
    uint8_t  entropy;    // entropy accumulator (0‑255)
    uint64_t last_addr;  // last physical address seen by this line
    uint64_t last_pc;    // last PC that accessed this line
};

static std::vector<EGAA_LineInfo> line_info;   // size = LLC_SETS * LLC_WAYS

/*-----------------------------------------------------------------
 *  Helper macros
 *-----------------------------------------------------------------*/
#define LINE_INDEX(set, way) ((set) * LLC_WAYS + (way))

// simple pop‑count (builtin works for gcc/clang)
inline uint8_t popcnt64(uint64_t x) {
    return static_cast<uint8_t>(__builtin_popcountll(x));
}

/*-----------------------------------------------------------------
 *  Initialize replacement state
 *-----------------------------------------------------------------*/
void InitReplacementState() {
    line_info.resize(LLC_SETS * LLC_WAYS);
    for (auto &li : line_info) {
        li.age      = 0;
        li.entropy  = 0;
        li.last_addr = 0;
        li.last_pc   = 0;
    }
}

/*-----------------------------------------------------------------
 *  Choose victim line in the set
 *-----------------------------------------------------------------*/
uint32_t GetVictimInSet(
    uint32_t cpu,
    uint32_t set,
    const BLOCK *current_set,
    uint64_t PC,
    uint64_t paddr,
    uint32_t type)
{
    uint32_t victim_way = 0;
    uint32_t max_score  = 0;   // larger score → more eligible for eviction

    for (uint32_t way = 0; way < LLC_WAYS; ++way) {
        const EGAA_LineInfo &li = line_info[LINE_INDEX(set, way)];
        // Effective age = age * (1 + entropy/256)
        uint32_t effective_age = li.age * (256 + li.entropy) / 256;

        // Prefer empty lines (valid == false) immediately
        if (!current_set[way].valid) {
            return way;
        }

        if (effective_age > max_score) {
            max_score   = effective_age;
            victim_way  = way;
        }
    }
    return victim_way;
}

/*-----------------------------------------------------------------
 *  Update replacement state after every access (hit or miss)
 *-----------------------------------------------------------------*/
void UpdateReplacementState(
    uint32_t cpu,
    uint32_t set,
    uint32_t way,
    uint64_t paddr,
    uint64_t PC,
    uint64_t victim_addr,
    uint32_t type,
    uint8_t hit)
{
    // -----------------------------------------------------------------
    // 1) Aging of *all* non‑accessed ways in the same set
    // -----------------------------------------------------------------
    const uint8_t ENTROPY_SCALE = 4;   // controls how fast entropy ages a line
    const uint8_t AGE_STEP_BASE = 1;   // base aging step per miss

    for (uint32_t w = 0; w < LLC_WAYS; ++w) {
        if (w == way) continue;                     // skip the accessed line
        EGAA_LineInfo &li = line_info[LINE_INDEX(set, w)];

        // Age increment = base + (entropy / ENTROPY_SCALE)
        uint8_t incr = AGE_STEP_BASE + (li.entropy >> ENTROPY_SCALE);
        li.age = (li.age + incr > 255) ? 255 : li.age + incr;
    }

    // -----------------------------------------------------------------
    // 2) Update the accessed line (way)
    // -----------------------------------------------------------------
    EGAA_LineInfo &my = line_info[LINE_INDEX(set, way)];

    // Reset age on hit or insertion
    my.age = 0;

    // Compute address entropy contribution
    uint64_t addr_diff = my.last_addr ^ paddr;      // XOR with previous address
    uint8_t delta_addr = popcnt64(addr_diff);      // number of differing bits

    // Compute PC entropy contribution
    uint64_t pc_diff   = my.last_pc ^ PC;
    uint8_t delta_pc   = popcnt64(pc_diff);

    // Combine both (weight PC a little less than address)
    uint16_t delta = delta_addr + (delta_pc >> 1);

    // Update entropy accumulator (saturating add, then decay a bit)
    uint16_t new_entropy = my.entropy + delta;
    if (new_entropy > 255) new_entropy = 255;
    // Decay half of the accumulated entropy on every access – helps after phase change
    my.entropy = static_cast<uint8_t>(new_entropy >> 1);

    // Store current address/PC for next round
    my.last_addr = paddr;
    my.last_pc   = PC;
}

/*-----------------------------------------------------------------
 *  Optional statistics (can be expanded later)
 *-----------------------------------------------------------------*/
void PrintStats() {
    // Example: average age & entropy over the whole cache
    uint64_t total_age = 0, total_entropy = 0;
    for (const auto &li : line_info) {
        total_age     += li.age;
        total_entropy += li.entropy;
    }
    double avg_age = double(total_age) / line_info.size();
    double avg_ent = double(total_entropy) / line_info.size();

    std::cout << "EGAA stats: avg_age = " << avg_age
              << ", avg_entropy = " << avg_ent << std::endl;
}

void PrintStats_Heartbeat() {
    // Could be left empty or used for periodic reporting
}