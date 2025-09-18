# AI-Generated Cache Replacement Policies with Groq API

This project explores whether **Large Language Models (LLMs)** can generate novel cache replacement policies automatically.  
Using the **Groq API**, I prompted multiple open-source models to produce candidate policies, integrated them into the **ChampSim CRC2** simulator, and evaluated their performance on SPEC CPU benchmarks.

---

## Project Overview

- **Goal**: Investigate if LLMs can design competitive cache replacement policies compared to traditional ones (LRU, SHiP, Hawkeye).  
- **Simulator**: ChampSim CRC2  
- **Benchmarks**: SPEC CPU traces (`astar`, `lbm`, `mcf`, `milc`, `omnetpp`)  
- **Metrics**: IPC (Instructions per Cycle), hit/miss rates, metadata overhead  
- **Models tested**: GPT-OSS (20B/120B), Kimi-K2, Qwen-3, DeepSeek R1, Meta-LLaMA Maverick/Scout, Gemma-2, etc.  
- **Best Policy Found**: **ChronoEntropy**, achieving the highest IPC (0.33) by balancing recency and predictability.

---

## Repository Structure

- **Code/** → Google Colab notebooks and scripts for running experiments  
- **CSV Files/** → Input traces and results  
- **Cache policies/** → Policy implementations (both AI-generated and baselines)  
- **Trace results (IPC Values)/** → Evaluation results for each policy  
- **Report.pdf** → Full project report with methodology, results, and analysis  

---

## How to Run

1. Open any notebook from the **Code/** folder in **Google Colab**.  
2. Upload it to your Colab account.  
3. Run all cells – dependencies will install automatically in Colab.  
4. Results (IPC values, hit rates, metadata stats) will be saved and can be compared with the baselines.  

---

## Key Findings

- LLMs can autonomously generate valid (sometimes competitive) cache replacement policies.  
- Compilation success rates varied across models (e.g., GPT-OSS-20B had 80% success, while some failed entirely).  
- Policies like **CAAR, EGAA, DAC, T-MAP** were competitive, but **ChronoEntropy** consistently gave the best IPC.  
- Metadata overhead is a practical concern — lightweight policies (like T-MAP and CAAR) fit hardware budgets better than heavier ones (EGAA, ChronoEntropy).  

---

## Author

**Amna Hassan**  
Department of Computer Science, UET Taxila  
📧 amnahassan.ahf@gmail.com  
