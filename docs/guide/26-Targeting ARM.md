**WORK IN PROGRESS**

**COMPILING FOR ARM IS AN EXPERIMENTAL FEATURE** because there are many different versions, variations, processors and available instructions. This compiler just provides a couple of options mainly meant for fun, experimentation, and learning.

The compiler itself cannot be compiled for ARM because it assumes little endian. The compiler can cross compile to ARM.



There are three categories of targets: Linux ARM, specific ARM processor, and general ARM. (this may change)


# Targeting general ARM
The target `arm` and `aarch64` is intended for compiling BTB code to ARM instructions unrelated to any processor and operating system. You may still be able to use this target if you are compiling for a specific processor. Targeting a specific processor may just add some compiler magic to make life easier. (compiling instructions unrelated to processor may not be possible, I am not sure)

If you are compiling for ARM then you probably have a specific build system and processor in mind where you want to generate object files and link using your own linker scripts. In this case you want BTB compiler to generate object files using a command like this: `btb -target arm main.btb -o main.o`.

If you want to test your code in QEMU then you can use: `btb -target arm main.btb -o main.elf` which will create a default linker script and link using `arm-none-eabi-ld` (or `aarch64-none-elf-ld`) and then run QEMU. You can add the `--what-is-done` flag to know exactly what the compiler is doing.

You can override which ARM toolchain is used using `--arm-tools arm-none-eabi` (don't use arm-none-eabi since that is the default).

**NOTE**: QEMU is an emulator. For the compiler to run it you must download QEMU and set PATH to point to the qemu binaries (`qemu-system-x`).

**NOTE**: When generating elf file for QEMU you may want to link with additional object files. This is not possible right now. You have to generate object files, create linker script and startup script and link those yourself.

**NOTE:** What is meant by "general ARM"? I don't know? ARMv7 or ARMv8? Names and things are work in progress.

# Targeting Linux ARM
**NOT SUPPPORTED YET**

# Targeting specific ARM processor
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

# Modules for ARM processors

implement prints and printc for UART for xilinx-synq-a9.
