import csv
import json
import re
import subprocess
from pathlib import Path

RISCV_GCC = "riscv64-unknown-elf-gcc"
RIPES = "/Users/sisi/Desktop/Ripes.app/Contents/MacOS/Ripes"

BASE_DIR = Path(__file__).resolve().parent
ROOT_DIR = BASE_DIR.parent

states_file = ROOT_DIR / "tests" / "export_d11_states.txt"

start_obj = ROOT_DIR / "_start.o"
ida_obj = ROOT_DIR / "ida_star.o"
pdb_obj = ROOT_DIR / "pdb_tables.o"

test_asm = ROOT_DIR / "batch_test_case.s"
test_obj = ROOT_DIR / "batch_test_case.o"
solver_elf = ROOT_DIR / "batch_solver.elf"
output_csv = ROOT_DIR / "d11_iret.csv"

states = [
    line.strip()
    for line in states_file.read_text().splitlines()
    if line.strip()
]

max_iret = 0
worst_state = None
failures = 0

with output_csv.open("w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["state", "exit_code", "retired"])

    for index, state in enumerate(states, 1):
        test_asm.write_text(f""".section .rodata
                                .globl input_state
                                input_state:
                                    .asciz "{state}"

                                .globl expected_length
                                expected_length:
                                    .byte 11
                                """
                            )

        subprocess.run(
            [
                RISCV_GCC, "-march=rv32i", "-mabi=ilp32",
                "-c", str(test_asm), "-o", str(test_obj),
            ],
            check=True,
        )

        subprocess.run(
            [
                RISCV_GCC, "-march=rv32i", "-mabi=ilp32",
                "-nostdlib", "-nostartfiles", "-Wl,-e,_start",
                str(start_obj), str(ida_obj), str(pdb_obj),
                str(test_obj), "-o", str(solver_elf),
            ],
            check=True,
        )

        result = subprocess.run(
            [
                RIPES, "--mode", "cli", "--src", str(solver_elf),
                "-t", "elf", "--proc", "RV32_ISS", "--iret", "--json",
            ],
            text=True,
            capture_output=True,
            check=False,
        )

        output = result.stdout + result.stderr

        exit_match = re.search(r"Program exited with code:\s*(\d+)", output)
        json_match = re.search(r'\{\s*"# instructions retired"\s*:\s*(\d+)\s*\}',output,re.S,)

        exit_code = int(exit_match.group(1)) if exit_match else -1
        retired = int(json_match.group(1)) if json_match else -1

        if exit_code != 0 or retired < 0:
            failures += 1

        writer.writerow([state, exit_code, retired])
        f.flush()

        if retired > max_iret:
            max_iret = retired
            worst_state = state

        if index % 100 == 0:
            print(
                f"[{index}/{len(states)}] "
                f"{state}  exit={exit_code}  iret={retired}  "
                f"max={max_iret}"
            )

print()
print(f"states tested: {len(states)}")
print(f"failures: {failures}")
print(f"worst state: {worst_state}")
print(f"max retired: {max_iret}")
print(f"under 50,000,000: {max_iret < 50_000_000}")