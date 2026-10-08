## Handwritten Assembly on Ripes
### Build the ELF executable

The handwritten implementation consists of multiple assembly files, so they are assembled and linked into a single ELF executable:

```bash
riscv64-unknown-elf-gcc \
    -march=rv32i -mabi=ilp32 \
    -nostdlib -nostartfiles \
    -Wl,-e,_start \
    _start.s ida_star.s utils/pdb_tables.s test_case.s \
    -o solver.elf
```
`-nostdlib` prevents the standard libraries from being linked, while
`-nostartfiles` prevents the default C runtime startup files from being used.
`-Wl,-e,_start` passes `-e _start` to the linker and sets `_start` as the ELF entry point.

### Run on Ripes CLI
```bash
/Ripes \
    --mode cli \
    --src solver.elf \
    -t elf \
    --proc RV32_ISS \
    --iret
```
Use `--proc RV32_5S` to change to pipeline model.
### Check the linked .text size
```bash
riscv64-unknown-elf-size -A solver.elf
```

## GCC Baseline on Ripes
The GCC baseline uses the same minimal `_start.s` as the handwritten implementation so that startup code does not bias the comparison.

### Build the GCC `-O2` ELF executable
```bash
riscv64-unknown-elf-gcc \
    -O2 -march=rv32i -mabi=ilp32 \
    -nostdlib -nostartfiles \
    -Wl,-e,_start \
    _start.s ida_star_target.c test_case.c \
    -o gcc_baseline.elf
```

### Run on Ripes CLI
```bash
./Ripes \
    --mode cli \
    --src gcc_baseline.elf \
    -t elf \
    --proc RV32_ISS \
    --iret
```
### Check the linked .text size
```bash
riscv64-unknown-elf-size -A gcc_baseline.elf
```

### Run LED on the Ripes
The GUI build enables the renderer and provides the LED Matrix base address to the GNU assembler:
```bash
riscv64-unknown-elf-gcc \
    -march=rv32i -mabi=ilp32 \
    -nostdlib -nostartfiles \
    -Wa,--defsym,LED_MATRIX_0_BASE=0xf0000000 \
    -Wl,-e,_start \
    _start.s ida_star.s utils/pdb_tables.s test_case.s led_matrix.s \
    -o led.elf
```

In Ripes:
1. Add an LED Matrix peripheral.
2. Set Width to 35 and Height to 25.
3. Load `led.elf`.
4. Run the program with RENDER=1.

The initial cube state is rendered before replay begins, and the display is redrawn after every completed HTM move.
For debugging, a breakpoint can be placed at the instruction corresponding to the renderer call or LED store in the current ELF.