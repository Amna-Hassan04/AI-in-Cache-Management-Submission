#include <vector>
#include <cstdint>
#include <algorithm>
#include <unordered_map>
#include "../inc/champsim_crc2.h"

#define NUM_CORE 1
#define LLC_SETS (NUM_CORE * 2048)
#define LLC_WAYS 16

// metadata per line
struct LineMeta {
    int8_t  tms;   // signed 6-bit Temporal-Marginality Score (-32..31)
    uint8_t rq;    // 2-bit reuse quantizer
};

static std::vector<std::vector<LineMeta>> meta(LLC_SETS, std::vector<LineMeta>(LLC_WAYS, {0,0}));

// PC history table: hash(pc) -> consecShortHits
struct PCEntry { uint8_t consecShortHits; };
static std::unordered_map<uint64_t, PCEntry> pcTable;

// per-set access counter (for fast aging)
static std::vector<uint64_t> setAccessCnt(LLC_SETS, 0);

// ------------------------------------------------------------------
void InitReplacementState() {
    for(auto &s : meta)
        for(auto &m : s) { m.tms = 0; m.rq = 0; }
    std::fill(setAccessCnt.begin(), setAccessCnt.end(), 0);
    pcTable.clear();
}
// ------------------------------------------------------------------
uint32_t GetVictimInSet(uint32_t cpu, uint32_t set, const BLOCK *current_set,
                        uint64_t PC, uint64_t paddr, uint32_t type) {
    int32_t minTms = 127;
    uint32_t victimWay = 0;
    for(uint32_t w=0; w<LLC_WAYS; ++w) {
        if(meta[set][w].tms < minTms) {
            minTms = meta[set][w].tms;
            victimWay = w;
        }
    }
    return victimWay;
}
// ------------------------------------------------------------------
void UpdateReplacementState(uint32_t cpu, uint32_t set, uint32_t way,
                            uint64_t paddr, uint64_t PC, uint64_t victim_addr,
                            uint32_t type, uint8_t hit) {
    setAccessCnt[set]++;

    LineMeta &m = meta[set][way];

    if(hit) {
        // --- hit path ---
        int add = (m.rq > 1) ? (2<<(m.rq-2)) : 1;
        m.tms = (int8_t)std::min<int>(31, m.tms + add);
        if(m.rq < 3) m.rq++;
    } else {
        // --- miss path (line just brought in) ---
        // decrement TMS for fast aging of all lines in the set
        for(uint32_t w=0; w<LLC_WAYS; ++w) {
            if(meta[set][w].tms > -32) meta[set][w].tms--;
        }
        // initialize new line
        uint64_t pcHash = PC & 0xFF; // 8-bit hash
        uint8_t &pcEnt = pcTable[pcHash].consecShortHits;
        m.tms = (pcEnt >= 2) ? 8 : 0;
        m.rq  = 0;
    }

    // slow aging: every 64 set accesses, decrement RQ for all lines
    if((setAccessCnt[set] & 0x3F) == 0) {
        for(uint32_t w=0; w<LLC_WAYS; ++w) {
            if(meta[set][w].rq > 0) meta[set][w].rq--;
        }
    }

    // update PC history table
    if(!hit && type==WRITEBACK) return; // ignore writeback fills for PC history
    uint64_t pcHash = PC & 0xFF;
    PCEntry &pe = pcTable[pcHash];
    if(hit) {
        // was this a "quick" re-hit? we approximate by checking
        // if TMS was positive (indicating prior credits)
        if(m.tms > 0 && pe.consecShortHits < 3) pe.consecShortHits++;
    } else {
        // missed: reset counter (streaming)
        pe.consecShortHits = 0;
    }
}
// ------------------------------------------------------------------
void PrintStats() {
    // (optional)
}
void PrintStats_Heartbeat() {
    // (optional)
}