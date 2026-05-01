import sys

def refine_traces(input_file, output_file, scale_count, target_max=2000000):
    commands = []

    # 1. Parse the input file
    with open(input_file, 'r') as f:
        for line in f:
            tokens = line.strip().split()
            if len(tokens) >= 2:
                cmd = tokens[0]
                try:
                    time_val = int(tokens[-1])
                    commands.append({"cmd": cmd, "time": time_val})
                except ValueError:
                    continue

    if len(commands) < 4:
        print("Error: Trace file must have at least 4 PIM_START commands.")
        return

    # Extract the first 4 PIM_START commands
    starts = commands[:4]
    rest = commands[4:]

    # [NEW] Calculate the total estimated computation time (sum of all raw PAUSE-RESUME latencies)
    total_compute_time = 0
    for j in range(0, len(rest), 2):
        if j + 1 < len(rest):
            pause_time = rest[j]["time"]
            resume_time = rest[j+1]["time"]
            total_compute_time += (resume_time - pause_time)

    # Shift the entire timeline so the very first command starts at tick 10.
    first_tick = starts[0]["time"]
    shift_to_10 = first_tick - 10
    for item in starts:
        item["time"] -= shift_to_10
    for item in rest:
        item["time"] -= shift_to_10

    # 2. Refinement 1: Merge Pause/Resume pairs
    merged_rest = []
    chunk_size = scale_count * 2

    for i in range(0, len(rest), chunk_size):
        chunk = rest[i : i + chunk_size]
        if not chunk:
            break

        first_pause_time = chunk[0]["time"]
        total_latency = 0

        # Iterate through the chunk in pairs of 2 (Pause, Resume)
        for j in range(0, len(chunk), 2):
            if j + 1 < len(chunk):
                pause_time = chunk[j]["time"]
                resume_time = chunk[j+1]["time"]
                total_latency += (resume_time - pause_time)

        merged_resume_time = first_pause_time + total_latency

        merged_rest.append({"cmd": "MEM_PAUSE", "time": first_pause_time})
        merged_rest.append({"cmd": "MEM_RESUME", "time": merged_resume_time})

    # 3. Refinement 2: Shrink the gap to 200
    if len(starts) > 0 and len(merged_rest) > 0:
        last_start_time = starts[-1]["time"]
        first_pause_time = merged_rest[0]["time"]

        current_gap = first_pause_time - last_start_time
        shift_amount = current_gap - 200

        # If the gap is larger than 200, shift all merged commands earlier
        if shift_amount > 0:
            for item in merged_rest:
                item["time"] -= shift_amount

    # 4. Refinement 3: Normalize execution phase to 2,000,000 max
    if len(merged_rest) > 1:
        e_start = merged_rest[0]["time"]
        e_end = merged_rest[-1]["time"]
        e_dur = e_end - e_start

        if e_dur > target_max:
            factor = target_max / float(e_dur)

            for i in range(1, len(merged_rest)):
                original_time = merged_rest[i]["time"]
                scaled_time = e_start + int((original_time - e_start) * factor)

                # Failsafe: Guarantee at least 1 tick between operations
                if scaled_time <= merged_rest[i-1]["time"]:
                    scaled_time = merged_rest[i-1]["time"] + 1

                merged_rest[i]["time"] = scaled_time

    # 5. Write out the refined trace
    with open(output_file, 'w') as f:
        # Write PIM_STARTs
        for item in starts:
            f.write(f"{item['cmd']} {item['time']}\n")

        # Write merged, shifted, and scaled PAUSE/RESUMEs
        for item in merged_rest:
            f.write(f"{item['cmd']} {item['time']}\n")

    return total_compute_time

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: python3 refine_trace.py <input_file> <output_file> <scale_count>")
        sys.exit(1)

    in_file = sys.argv[1]
    out_file = sys.argv[2]
    scale = int(sys.argv[3])

    total_time = refine_traces(in_file, out_file, scale)

    if total_time is not None:
        print(f"Success! Trace normalized to 2,000,000 cycles and saved to {out_file}.")
        print(f"--------------------------------------------------")
        print(f"Reference Estimation (Total compute latency): {total_time} ticks/cycles")
        print(f"--------------------------------------------------")
