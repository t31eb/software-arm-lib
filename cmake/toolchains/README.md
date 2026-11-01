# CMake toolchain files

| File | Toolchain | Notes |
|---|---|---|
| `arm-none-eabi_mcuxpresso.cmake` | arm-none-eabi from an MCUXpresso IDE installation | Default in all `CMakePresets.json`. Needs `MCUXPRESSO_TOOLCHAIN_PATH` or `-DTOOLCHAIN_PREFIX=...` and the LinkServer binaries. |
| `arm-none-eabi_gnu.cmake` | Plain GNU arm-none-eabi from `PATH` | No MCUXpresso installation needed, e.g. for headless Linux build machines and CI. |

Both files share the compiler, linker and assembler flags in `arm-none-eabi_mcuxpresso/`.

## Using the GNU toolchain (Debian/Ubuntu)

```sh
sudo apt install cmake ninja-build gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
cd sblib
cmake --preset arm -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/arm-none-eabi_gnu.cmake
cmake --build --preset release-arm
```

The linker scripts reference the MCUXpresso library `libcr_newlib_nohost.a`.
The GNU toolchain file provides newlib's `libnosys.a` under that name.

## Limitations of the GNU toolchain file

- Code size differs from MCUXpresso builds (other GCC version and C library build).
  Check the memory usage printed by the linker. Example: the bootloader built with
  Debian's arm-none-eabi-gcc 13.2 needs 12904 bytes and does **not** fit into its 12 KiB (0x3000) region.
  Use the MCUXpresso toolchain or the released binary for the bootloader.
- No flashing/debugging targets (no LinkServer). Use e.g. OpenOCD or pyOCD with a CMSIS-DAP probe.
