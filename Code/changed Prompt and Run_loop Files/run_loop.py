#!/usr/bin/env python3
import sys, os, re, time, sqlite3, subprocess
from pathlib import Path
from typing import Optional, Tuple
from dotenv import load_dotenv
from groq import Groq   # ✅ switched to Groq
from RAG import ExperimentRAG
from PromptGenerator import PolicyPromptGenerator

# ───────────────────────────────────────────
# Configuration
# ───────────────────────────────────────────
DB_PATH = "DB/funsearch.db"
LIB_PATH = "ChampSim_CRC2/lib/config1.a"
INCLUDE_DIR = "ChampSim_CRC2/inc"
EXAMPLE_DIR = Path("ChampSim_CRC2/new_policies")

WARMUP_INST = "1000000"
SIM_INST = "10000000"
ITERATIONS = 5   # ✅ capped at 5

EXAMPLE_DIR.mkdir(parents=True, exist_ok=True)

workloads = [
    {"name": "astar", "trace_path": "ChampSim_CRC2/traces/astar_313B.trace.gz"},
    {"name": "lbm", "trace_path": "ChampSim_CRC2/traces/lbm_564B.trace.gz"},
    {"name": "mcf", "trace_path": "ChampSim_CRC2/traces/mcf_250B.trace.gz"},
    {"name": "milc", "trace_path": "ChampSim_CRC2/traces/milc_409B.trace.gz"},
    {"name": "omnetpp", "trace_path": "ChampSim_CRC2/traces/omnetpp_17B.trace.gz"}
]

# Models to test (Groq supported)
MODEL_LIST = [
    "openai/gpt-oss-20b",
    "meta-llama/llama-4-maverick-17b-128e-instruct",
    "meta-llama/llama-4-scout-17b-16e-instruct",
    "llama-3.1-8b-instant",
    "llama-3.3-70b-versatile",
    "deepseek-r1-distill-llama-70b",
    "gemma2-9b-it",
    "qwen/qwen3-32b",
    "moonshotai/kimi-k2-instruct",
    "moonshotai/kimi-k2-instruct-0905",
    "openai/gpt-oss-120b",
]


# ───────────────────────────────────────────
# Helpers
# ───────────────────────────────────────────
def sanitize(name: str) -> str:
    return "".join(c if c.isalnum() else "_" for c in name).strip("_").lower()

def parse_policy_content(text: str) -> Tuple[Optional[str], Optional[str], Optional[str]]:
    def _extract(pattern: str):
        m = re.search(pattern, text, flags=re.DOTALL | re.IGNORECASE)
        return m.group(1).strip() if m else None
    return (
        _extract(r"##\s*Policy\s*Name\s*\n(.*?)\n"),
        _extract(r"##\s*Policy\s*Description\s*\n(.*?)\n"),
        _extract(r"```cpp\s*(.*?)\s*```"),
    )

def compile_policy(cc: Path) -> Path:
    exe = cc.with_suffix(".out")
    subprocess.run(
        ["g++", "-Wall", "-std=c++17", f"-I{INCLUDE_DIR}", str(cc), LIB_PATH, "-o", str(exe)],
        check=True,
    )
    return exe

def run_policy(exe: Path, trace_path: Path) -> str:
    res = subprocess.run(
        [str(exe), "-warmup_instructions", WARMUP_INST,
         "-simulation_instructions", SIM_INST, "-traces", str(trace_path)],
        check=True, capture_output=True, text=True,
    )
    return res.stdout

def parse_hit_rate(output: str) -> float:
    m = re.search(r"LLC TOTAL\s+ACCESS:\s+(\d+)\s+HIT:\s+(\d+)", output)
    if not m:
        raise RuntimeError("LLC TOTAL not found")
    return int(m.group(2)) / int(m.group(1))

def record(workload, name, desc, cc: Path, rate, workload_desc):
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute(
        """INSERT INTO experiments
           (workload, policy, policy_description, workload_description,
            cpp_file_path, cache_hit_rate, score)
           VALUES (?, ?, ?, ?, ?, ?, ?)""",
        (workload, name, desc, workload_desc, str(cc), rate, rate),
    )
    conn.commit(); conn.close()

# ───────────────────────────────────────────
# Main Loop
# ───────────────────────────────────────────
def run_experiment(model_name: str):
    print(f"\n🚀 Running experiment with {model_name}\n")

    rag = ExperimentRAG(DB_PATH)
    prompt_gen = PolicyPromptGenerator(DB_PATH)
    load_dotenv(dotenv_path=Path(".env"), override=False)

    client = Groq(api_key=os.getenv("GROQ_API_KEY"))

    top_policies = rag.get_top_policies_by_score("all", top_n=5)
    workload_desc, traces = rag.get_all_workloads_with_description_and_traces()
    best_hit = top_policies[0]["score"]

    prev_name = prev_desc = prev_code = None

    for i in range(ITERATIONS):
        # Base prompt (forces novelty)
        if i == 0:
            prompt = (
                f"The following workloads are under consideration:\n{workload_desc}\n\n"
                "The top-performing cache replacement policies from past experiments are:\n"
                + "\n".join(
                    f"- {p['policy']} (Hit Rate: {float(p['score']):.2%})"
                    for p in top_policies
                )
                + "\n\n"
                "Task: Propose a **completely new and unique** cache replacement policy. "
                "Do NOT repeat or reuse any previous idea or implementation. "
                "Think creatively and ensure novelty.\n\n"
                "Provide output in the format:\n\n"
                "## Policy Name\n<name>\n\n"
                "## Policy Description\n<description>\n\n"
                "## C++ Implementation\n"
                f"{prompt_gen._get_code_template()}\n"
            )
        else:
            prompt = (
                f"Your previous policy was **{prev_name}**:\n{prev_desc}\n\n"
                "Do NOT repeat the same idea again. Propose a new unique replacement policy. "
                "You may combine novel heuristics, statistical tracking, or hybrid approaches. "
                "Ensure it is different from all prior policies.\n\n"
                "Provide in the format:\n\n"
                "## Policy Name\n<name>\n\n"
                "## Policy Description\n<description>\n\n"
                "## C++ Implementation\n"
                f"{prompt_gen._get_code_template()}\n"
            )

        # Call Groq
        resp = client.chat.completions.create(
            model=model_name,
            messages=[{"role": "system", "content": "You are a cache replacement policy generator."},
                      {"role": "user", "content": prompt}],
            temperature=0.9,  # encourage novelty
        )

        text = resp.choices[0].message.content

        # Parse response
        name, desc, code = parse_policy_content(text)
        if not (name and desc and code):
            print("❌ Failed to parse model output.")
            continue

        cc = EXAMPLE_DIR / f"{i:03}_{sanitize(name)}.cc"
        cc.write_text(code, encoding="utf-8")

        try:
            exe = compile_policy(cc)
        except subprocess.CalledProcessError:
            print("❌ Compile error, skipping...")
            continue

        # Evaluate across workloads
        total = 0
        for trace in workloads:
            out = run_policy(exe, trace["trace_path"])
            rate = parse_hit_rate(out)
            total += rate
            record(trace["name"], name, desc, cc, rate, "")
            print(f"   [+] {name} → {trace['name']} → {rate:.2%}")

        avg_hit = total / len(workloads)
        record("all", name, desc, cc, avg_hit, "")
        print(f"✅ Iteration {i} ({model_name}): {name} → Avg Hit Rate {avg_hit:.2%}\n")

        prev_name, prev_desc, prev_code = name, desc, code

    rag.close(); prompt_gen.close()

# ───────────────────────────────────────────
# Run Across All Models
# ───────────────────────────────────────────
if __name__ == "__main__":
    for model in MODEL_LIST:
        run_experiment(model)
