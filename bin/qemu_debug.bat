ECHO OFF
SET QEMU_DIR=D:\00_soft\qemu-2.5.0
START %QEMU_DIR%\qemu-system-x86_64w.exe -L %QEMU_DIR% -kernel kernel.sys -gdb tcp::1234,ipv4 -S
REM START %QEMU_DIR%\qemu-system-x86_64w.exe -L %QEMU_DIR% -kernel kernel.sys -serial COM3
