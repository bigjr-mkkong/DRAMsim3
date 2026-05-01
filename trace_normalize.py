import sys
import random

def process_trace(input_file, output_file, cpu_period_ns=1.0):
    lines_data = []

    # 1. Parse the raw trace
    with open(input_file, 'r') as f:
        for line in f:
            tokens = line.strip().split()
            if not tokens or len(tokens) < 2:
                continue

            try:
                raw_ts = int(tokens[-1])
            except ValueError:
                continue

            # Extract the command and address
            if tokens[-2].upper() in ["READ", "WRITE"]:
                cmd = f"{tokens[-3]} {tokens[-2].upper()}"
            else:
                cmd = tokens[-2]

            lines_data.append({"cmd": cmd, "raw_ts": raw_ts})

    if not lines_data:
        print("Error: No valid trace data found.")
        return

    # 2. Normalize and apply random jitter
    min_ts = lines_data[0]["raw_ts"]
    randomized_data = []

    for item in lines_data:
        # Normalize the base timeline so the first event is centered at 100ns.
        # This prevents the [-50, +50] offset from generating negative time.
        norm_ts = (item["raw_ts"] - min_ts) + 100

        # Apply random jitter to simulate the actual arrival time inside the window.
        # This gives a range equivalent to [50, 150] for the first 100ns tick.
        jittered_ts_ns = norm_ts + random.uniform(-50, 50)

        randomized_data.append({
            "cmd": item["cmd"],
            "ns": jittered_ts_ns
        })

    # 3. Sort to guarantee chronological order
    # Randomization might cause adjacent 100ns windows to bleed into each other slightly.
    randomized_data.sort(key=lambda x: x["ns"])

    # 4. Convert to cycles and output
    with open(output_file, 'w') as f:
        for item in randomized_data:
            # Convert the nanosecond timestamp to CPU/MEM clock cycles
            cycle = int(item["ns"] / cpu_period_ns)
            f.write(f"{item['cmd']} {cycle}\n")

if __name__ == "__main__":
    in_file = "raw_trace.txt" if len(sys.argv) < 2 else sys.argv[1]
    out_file = "cycle_trace.txt" if len(sys.argv) < 3 else sys.argv[2]

    # SET YOUR CLOCK PERIOD HERE:
    # 1.0 ns = 1 GHz
    # 0.5 ns = 2 GHz
    # 2.0 ns = 500 MHz
    CPU_PERIOD_NS = 0.83

    process_trace(in_file, out_file, CPU_PERIOD_NS)
    print(f"Success! Trace converted to cycles and saved to: {out_file}")
