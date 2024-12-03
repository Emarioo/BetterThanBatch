**COMPILING FOR ARM IS AN EXPERIMENTAL FEATURE**. The generated instruction set is ARMv7-A (even if ARM.attributes in the generated object file says otherwise).

# Target arm (ARMv7-A, 32-bit)
If you are compiling for ARM then you probably have a specific processor in mind where you want to the compiler to generate an object file and link using your own linker scripts. In this case use: `btb -target arm main.btb -o main.o`.

If you want to test your code in QEMU (xilinx-zynq-a9) then you can use: `btb -target arm main.btb -o main.elf` which will create a default linker script and link using `arm-none-eabi-ld` and then run QEMU. You can add `--qemu-gdb` to start QEMU with a paused GDB server allowing you to connect to it using `target remote :1234` in GDB.

**NOTE**: QEMU is an emulator. For the compiler to run it you must download QEMU and set PATH to point to the qemu binaries (`qemu-system-arm`).

**NOTE**: When generating ELF file for QEMU you may want to link with additional object files. You can add `#link "thing.o"` inside your source code. You can also pass any arbitrary flag to `arm-none-eabi-ld` using this.


# Target aarch-64 (ARMv8, 64-bit)
**NOT SUPPPORTED YET**

# Target Linux ARM
**NOT SUPPPORTED YET**

# Useful commands
Read about QEMU arguments here: https://www.qemu.org/docs/master/system/qemu-manpage.html

```bash
# Emulate main.elf in QEMU
qemu-system-arm -semihosting -nographic -serial mon:stdio -M xilinx-zynq-a9 -cpu cortex-a9 -kernel main.elf

# Debug main.elf in QEMU and create a server for gdb to connect to
qemu-system-arm -semihosting -nographic -serial mon:stdio -M xilinx-zynq-a9 -cpu cortex-a9 -kernel main.elf -s -S

# Debug QEMU using gdb (in a second terminal)
arm-none-eabi-gdb main.elf -ex "target remote :1234"
```

# Modules for ARM
The standard library that use operating system functions will not work when targeting ARM. So will certain features such as atomic intrinsics.

There is `xilinx-synq-a9.btb` which is a module that contains functions to interract with the UART.

# Future
The plan is to have three categories of target, ARM for Linux, and these ARM versions: ARMv7-A (32-bit), ARMv7-M (32-bit), ARMv8-? (64-bit).
