# Faulty Module: Kernel Oops Analysis

**Event:** Intentional Kernel Crash
**Trigger:** `echo "data" > /dev/faulty`
**Error:** NULL pointer dereference at address `0000000000000000`

### Root Cause
The `faulty_write` function in the driver attempts to write to a NULL address, triggering a **Data Abort (DABT)**. This causes the kernel to "Oops," killing the calling process (the shell).

### Key Trace Data
- **Location:** `faulty_write+0x10/0x20`
- **CPU State:** `pc : faulty_write+0x10/0x20 [faulty]`
- **Kernel Version:** 6.1.44
- **Taint Status:** `O` (Out-of-tree module)

### Call Stack
1. `faulty_write` (Crash point)
2. `ksys_write`
3. `vfs_write`
4. `el0_svc` (System call entry)
