from typing import List
from RAG import ExperimentRAG
import os

class PolicyPromptGenerator:
    def __init__(self, db_path: str = 'funsearch.db'):
        self.rag = ExperimentRAG(db_path)

    def _get_code_template(self) -> str:
        """Minimal but strict C++ template matching ChampSim CRC2 interface"""
        return '''```cpp
#include <vector>
#include <cstdint>
#include <iostream>
#include "../inc/champsim_crc2.h"

#define NUM_CORE 1
#define LLC_SETS (NUM_CORE * 2048)
#define LLC_WAYS 16

// Initialize replacement state
void InitReplacementState() {
    // --- REQUIRED: initialize any metadata structures here ---
}

// Choose victim line in the set
uint32_t GetVictimInSet(
    uint32_t cpu,
    uint32_t set,
    const BLOCK *current_set,
    uint64_t PC,
    uint64_t paddr,
    uint32_t type
) {
    // --- REQUIRED: choose victim line index ---
    return 0;
}

// Update replacement state
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
    // --- REQUIRED: update metadata ---
}

// Print end-of-simulation statistics
void PrintStats() {
    // --- OPTIONAL: print final stats ---
}

// Print periodic statistics
void PrintStats_Heartbeat() {
    // --- OPTIONAL: print progress stats ---
}
```'''

    def _read_policy_code(self, file_path: str) -> str:
        """Read an existing policy C++ file."""
        try:
            with open(file_path, 'r') as f:
                return f.read()
        except FileNotFoundError:
            return "// File missing"

    def generate_prompt(self, workload: str, top_n: int = 2) -> str:
        """
        Generate a structured prompt for the LLM:
        - Includes top performing policies
        - Explicitly asks for a novel design
        """
        top_policies = self.rag.get_top_policies_by_cache_hit(workload, top_n=top_n)
        if not top_policies:
            return f"No prior policies found for workload: {workload}"

        # Workload section
        parts: List[str] = [
            "You are a cache replacement policy design expert.\n",
            "Analyze the workload below and the best past policies, then create a **new and unique** replacement policy.\n\n",
            "# Workload\n",
            f"Name: {workload}\n",
            f"Description: {top_policies[0]['workload_description']}\n\n",
            "# Past Top Policies\n"
        ]

        # Show top policies with code
        for i, policy in enumerate(top_policies, 1):
            code = self._read_policy_code(policy['cpp_file_path'])
            parts += [
                f"### Policy {i}\n",
                f"- Name: {policy['policy']}\n",
                f"- Hit Rate: {policy['cache_hit_rate']:.2%}\n",
                f"- Description: {policy['policy_description']}\n",
                "Implementation:\n",
                f"```cpp\n{code}\n```\n\n"
            ]

        # Task instruction with novelty enforcement
        parts += [
            "# Task\n",
            "Design a **novel cache replacement policy** that:\n",
            "- Is fundamentally different from all above examples.\n",
            "- Uses innovative heuristics, statistical features, or hybrid logic.\n",
            "- Considers workload properties (locality, reuse distance, phase changes, branching).\n",
            "- Respects storage/area budget constraints (~≤64KB extra metadata).\n",
            "- Is robust across *all* workloads.\n\n",
            "Your response MUST follow this format:\n",
            "## Policy Name\n[Short, unique name]\n\n",
            "## Policy Description\n[One-paragraph explanation of the intuition and novelty]\n\n",
            "## C++ Implementation\n",
            self._get_code_template(),
            "\n# Guidelines\n",
            "1. Include \"../inc/champsim_crc2.h\" at the top.\n",
            "2. Implement all five functions.\n",
            "3. Do NOT bypass WRITEBACK accesses.\n",
            "4. If possible, add comments to explain design choices.\n",
            "5. Ensure code compiles cleanly with g++ -std=c++17.\n",
            "6. Never repeat or slightly modify an earlier policy — it must be a new idea.\n"
        ]

        return ''.join(parts)

    def close(self):
        self.rag.close()


# Example usage
if __name__ == '__main__':
    generator = PolicyPromptGenerator('funsearch.db')
    try:
        print(generator.generate_prompt('Astar'))
    finally:
        generator.close()
