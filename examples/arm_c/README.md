Download ARM toolchain: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads

Download QEMU: https://www.qemu.org/download/#windows

(change PATH environment variable)

Then run these:
```bat
cd examples/arm
run.bat
```

Convert run.bat to run.sh if you are on Linux.

If QEMU is stuck and Ctrl+C doesn't work try Ctrl+A (release) X

# Resources

https://stackoverflow.com/questions/60552355/qemu-baremetal-emulation-how-to-view-uart-output

https://github.com/rlangoy/ZedBoard-BareMetal-Examples/blob/master/Hello05

https://stackoverflow.com/questions/31990487/how-to-cleanly-exit-qemu-after-executing-bare-metal-program-without-user-interve