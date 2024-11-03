**WORK IN PROGRESS**

There are three categories of targets: Linux ARM, specific ARM processor, and general ARM.


# Targeting general ARM
The target `arm` and `aarch64` is intended for compiling BTB code to ARM instructions unrelated to any processor and operating system. You may still be able to use this target if you are compiling for a specific processor. Targeting a specific processor may just add some compiler magic to make life easier.

If you are compiling for ARM then you probably have a specific build system and processor in mind where you want to generate object files and link using your own linker scripts. In this case you want BTB compiler to generate object files using a command like this: `btb -target arm main.btb -o main.o`.

If you want to test your code in QEMU then you can use: `btb -target arm main.btb -o main.elf` which will create a default linker script and link using `arm-none-eabi-ld` (or `aarch64-none-elf-ld`) and then run QEMU. You can add the `--what-is-done` flag to know exactly what the compiler is doing.

**NOTE**: QEMU is an emulator.

**NOTE**: When generating elf file for QEMU you may want to link with additional object files. This is not possible right now. You have to generate object files, create linker script and startup script and link those yourself.

**NOTE:** What is meant by "general ARM"? I don't know? ARMv7 or ARMv8? Names and things are work in progress.

# Targeting Linux ARM
**NOT SUPPPORTED YET**

# Targeting specific ARM processor
**NOT SUPPPORTED YET**

prints and printc is implemented to use UART for xilinx-synq-a9.
