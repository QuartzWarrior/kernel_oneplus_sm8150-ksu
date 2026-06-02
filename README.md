# OnePlus 7 Pro (guacamole) - YAAP 16 Kernel with SukiSU-Ultra + SUSFS

Linux 4.14.336 for the OnePlus 7 Pro (SM8150 / `guacamole`) running [YAAP 16](https://github.com/yaap), with [SukiSU-Ultra](https://github.com/SukiSU-Ultra/SukiSU-Ultra) (`builtin` branch, non-GKI) and [SUSFS](https://gitlab.com/simonpunk/susfs4ksu) (`kernel-4.14` branch, v1.5.5) fully integrated.

> **Looking for KernelSU-Next instead?** See the [`sixteen-ksu`](../../tree/sixteen-ksu) branch.

---

## Device

| Key | Value |
|---|---|
| Device | OnePlus 7 Pro (`guacamole`) |
| Codename | neptune |
| SoC | Qualcomm SM8150 (Snapdragon 855) |
| Kernel base | [yaap/kernel_oneplus_sm8150](https://github.com/yaap/kernel_oneplus_sm8150) `sixteen` branch |
| Kernel version | 4.14.336 |
| ROM | YAAP 16 (Android 16, API 36) |

---

## What's included

- **SukiSU-Ultra** (`builtin` branch, v4.1.3) -> non-GKI manual hooks
- **SUSFS v1.5.5** (`kernel-4.14` branch) -> root hiding
- All six hook sites patched: `fs/exec.c`, `fs/open.c`, `fs/read_write.c`, `fs/stat.c`, `kernel/reboot.c`, `drivers/input/input.c`
- KPM disabled (requires Linux 5.0+ APIs)
- selinux_hide disabled (requires Linux 5.10+ kernel structures)
- Built-in cert allowlist accepts all major SukiSU/KernelSU manager variants, no custom signing required

### What SUSFS hides

| Feature | Config |
|---|---|
| Root files and paths (`/su`, etc.) | `CONFIG_KSU_SUSFS_SUS_PATH=y` |
| Modified inode stats | `CONFIG_KSU_SUSFS_SUS_KSTAT=y` |
| KSU default mounts | `CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT=y` |
| `/proc/cmdline` / bootconfig | `CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=y` |
| KSU/SUSFS symbols in `/proc/kallsyms` | (via SUSFS patch to `kernel/kallsyms.c`) |

> **Note:** "Hide SELinux modifications" in the manager cannot be enabled as it requires kernel 5.10+ internals. Use the [TreatWheel](https://github.com/PerformanC/Treat-Wheel-Zygisk) module (via [ReZygisk](https://github.com/PerformanC/ReZygisk)) as an alternative for banking-app-grade SELinux hiding.

---

## Build requirements

| Tool | Where to get |
|---|---|
| Clang `r547379` (clang 20) | [android-clang](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/) -> place at `../android-clang/clang-r547379/` relative to this repo |
| `arm-linux-gnueabi-*` | `apt install gcc-arm-linux-gnueabi` |
| `aarch64-linux-gnu-*` | `apt install gcc-aarch64-linux-gnu` |
| Python 3, `mkbootimg` | `apt install python3 mkbootimg` |

---

## Building the kernel

```bash
./build.sh
```

Output: `arch/arm64/boot/Image.gz-dtb`

---

## Manager

Download the official SukiSU-Ultra manager APK from the [SukiSU-Ultra releases page](https://github.com/SukiSU-Ultra/SukiSU-Ultra/releases). The kernel's cert allowlist already includes the official cert. It also accepts KernelSU-Next, KernelSU, RKSU, MKSU, and KowSU manager APKs.

---

## Repacking the boot image

You need the original `boot.img` from your device, I personally grabbed mine from [yaap's codebucket](https://mirror.codebucket.de/yaap/guacamole/).

Extract the ramdisk and repack:

```python
python3 - <<'EOF'
import struct
with open('boot.img', 'rb') as f:
    data = f.read()
page_size = struct.unpack('<I', data[36:40])[0]
kernel_size = struct.unpack('<I', data[8:12])[0]
ramdisk_size = struct.unpack('<I', data[16:20])[0]
def pages(n, p): return (n + p - 1) // p
ramdisk_offset = page_size + pages(kernel_size, page_size) * page_size
with open('ramdisk.img', 'wb') as f:
    f.write(data[ramdisk_offset:ramdisk_offset + ramdisk_size])
print(f'Extracted {ramdisk_size} byte ramdisk')
EOF

mkbootimg \
  --header_version 1 \
  --kernel arch/arm64/boot/Image.gz-dtb \
  --ramdisk ramdisk.img \
  --base 0x00000000 --kernel_offset 0x00008000 \
  --ramdisk_offset 0x01000000 --tags_offset 0x00000100 \
  --pagesize 4096 \
  --os_version "16.0.0" --os_patch_level "2026-05" \
  --cmdline "androidboot.hardware=qcom androidboot.console=ttyMSM0 androidboot.memcg=1 lpm_levels.sleep_disabled=1 msm_rtb.filter=0x237 service_locator.enable=1 swiotlb=2048 loop.max_part=7 androidboot.usbcontroller=a600000.dwc3 kpti=off androidboot.vbmeta.avb_version=1.0 buildvariant=user" \
  --output boot-ksu.img
```

Flash:

```bash
fastboot flash boot boot-ksu.img
```

---

## Changes vs upstream YAAP kernel

### Kernel hook sites

| File | What changed |
|---|---|
| `fs/exec.c` | Added `ksu_handle_execveat()` at the top of `do_execveat_common()` |
| `fs/open.c` | Added `ksu_handle_faccessat()` in `SYSCALL_DEFINE3(faccessat, ...)` |
| `fs/read_write.c` | Added `ksu_handle_vfs_read()` in `vfs_read()` |
| `fs/stat.c` | Added `ksu_handle_stat()` in `vfs_statx()`, plus SUSFS `susfs_sus_kstat_spoof_generic_fillattr()` call in `generic_fillattr()` |
| `kernel/reboot.c` | Added `ksu_handle_sys_reboot()` at the top of `SYSCALL_DEFINE4(reboot, ...)` |
| `drivers/input/input.c` | Added `ksu_handle_input_handle_event()` in `input_handle_event()` |
| `drivers/Kconfig` | Added `source "drivers/kernelsu/Kconfig"` |
| `drivers/Makefile` | Added `obj-$(CONFIG_KSU) += kernelsu/` |

### SUSFS kernel patches

Applied from the [`kernel-4.14` branch](https://gitlab.com/simonpunk/susfs4ksu/-/tree/kernel-4.14) of simonpunk/susfs4ksu. New source files added:

| File | Purpose |
|---|---|
| `fs/susfs.c` | Core SUSFS implementation |
| `fs/sus_su.c` | SUS su helper |
| `include/linux/susfs.h` | SUSFS API declarations |
| `include/linux/susfs_def.h` | SUSFS constants and inline helpers |
| `include/linux/sus_su.h` | sus_su header |
| `include/linux/susfs_ksu_compat.h` | Bridge header (see below) |

Existing files patched by the SUSFS `50_add_susfs_in_kernel-4.14.patch`:

| File | What changed |
|---|---|
| `fs/Makefile` | Added `obj-$(CONFIG_KSU_SUSFS) += susfs.o` |
| `fs/dcache.c` | SUS_PATH check in `__d_lookup_rcu()` |
| `fs/namei.c` | SUS_PATH checks across path lookup functions |
| `fs/namespace.c` | SUS_MOUNT handling and auto-add KSU default mount |
| `fs/notify/fdinfo.c` | SUS_MOUNT fdinfo hiding |
| `fs/overlayfs/inode.c`, `overlayfs.h`, `readdir.c`, `super.c`, `util.c` | Overlay FS SUS integration |
| `fs/proc/cmdline.c` | `SPOOF_CMDLINE_OR_BOOTCONFIG` hook |
| `fs/proc/fd.c` | Open-redirect fd spoofing |
| `fs/proc/task_mmu.c` | SUS_KSTAT include for memory map spoofing |
| `fs/proc_namespace.c` | SUS_MOUNT namespace hiding |
| `fs/readdir.c` | SUS_PATH directory entry hiding |
| `fs/statfs.c` | Open-redirect statfs spoofing |
| `include/linux/mount.h` | `susfs_mnt_id_backup` field in `vfsmount` |
| `include/linux/sched.h` | `susfs_task_state` and `susfs_last_fake_mnt_id` fields in `task_struct` |
| `kernel/kallsyms.c` | Hides `ksu_*`, `susfs_*`, `ksud` symbols from `/proc/kallsyms` |
| `kernel/sys.c` | `susfs_spoof_uname()` hook in `sys_newuname()` |

### Build fixes

| File | What changed |
|---|---|
| `arch/arm64/kernel/vdso32/Makefile` | `LD_COMPAT ?= $(LD)` -> uses `arm-linux-gnueabi-ld` instead of the host `ld` which has no ARM target |
| `lib/Kconfig` | Added `select ZSTD_COMMON` to fix undefined ZSTD symbols at link time |

### defconfig (`arch/arm64/configs/neptune_defconfig`)

| Config | Reason |
|---|---|
| `CONFIG_KSU=y` | SukiSU-Ultra |
| `CONFIG_KSU_DEBUG=y` | Debug logging -> remove for release builds |
| `CONFIG_KALLSYMS_ALL=y` | Required by SukiSU-Ultra for non-GKI |
| `# CONFIG_KPM is not set` | KPM uses Linux 5.0 APIs not available on 4.14 |
| `CONFIG_KSU_SUSFS=y` | SUSFS root hiding |
| `CONFIG_KSU_SUSFS_SUS_PATH=y` | Hide suspicious paths |
| `CONFIG_KSU_SUSFS_SUS_MOUNT=y` | Hide suspicious mounts |
| `CONFIG_KSU_SUSFS_SUS_KSTAT=y` | Spoof inode stats |
| `CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG=y` | Spoof `/proc/cmdline` |
| `CONFIG_KSU_SUSFS_AUTO_ADD_SUS_KSU_DEFAULT_MOUNT=y` | Auto-hide KSU mounts |
| `CONFIG_CPU_INPUT_BOOST=y` | Required -> `fork.c` and `sched/core.c` reference symbols only in `cpu_input_boost.c` |

### SukiSU-Ultra internal patches (`KernelSU/kernel/`)

**`KernelSU/kernel/runtime/ksud.c`** -> throne tracking + SELinux setup

`on_post_fs_data()` fires when Zygote starts. In the built-in path without ksud pre-installed, `track_throne()` was never called and `ksu_cred` had no proper SELinux context. Fixed by calling `apply_kernelsu_rules()` + `cache_sid()` + `setup_ksu_cred()` then `track_throne()` at the start, plus a 3-second delayed retry. All `ksu_selinux_hide_*` calls guarded with `LINUX_VERSION_CODE >= 5.10` (unconditional in the builtin branch, causes implicit-declaration errors on 4.14).

**`KernelSU/kernel/hook/lsm_hook.c`** -> prctl detection + fd bootstrap

Added `task_prctl` LSM hook for `prctl(0xDEADBEEF, 2, ...)` to break the circular dependency where the ioctl fd couldn't be installed without throne tracking having already run.

**`KernelSU/kernel/feature/sucompat.c`** -> manager root bootstrap

Added `|| is_uid_manager(current_uid().val)` to `__is_su_allowed()` so the manager can get a root shell before it has explicitly granted itself root (allowlist is empty on fresh install).

**`KernelSU/kernel/manager/pkg_observer.c`** -> packages.list observer

Widened `MASK_SYSTEM` with `FS_MODIFY | FS_CLOSE_WRITE` so direct writes to `packages.list` trigger a throne re-scan.

**`KernelSU/kernel/sulog/event.c`** -> 4.14 compatibility fixes

`strncpy_from_user_nofault` (added in 5.8) replaced with `strncpy_from_user`. `USER_ARG_NULL` pointer/value mismatch fixed for the `CONFIG_KSU_SUSFS` code path.

**`KernelSU/kernel/supercall/dispatch.c`** -> SUSFS API bridge

Added `#include <linux/susfs_ksu_compat.h>` under `CONFIG_KSU_SUSFS`. This header bridges the version mismatch between SukiSU-Ultra's `builtin` branch (written for SUSFS v2.1.0) and the only available 4.14-compatible patches (SUSFS v1.5.5). The differences are:
- v2.1.0 uses `void __user **` function signatures; v1.5.5 uses typed struct pointers
- v2.1.0 uses `i_mapping->flags` + thread flags for state; v1.5.5 uses `inode->i_state` bits + `task_struct.susfs_task_state`
- v2.1.0 adds ~10 new functions not in v1.5.5 (loop path add, sus_map, AVC log spoofing, etc.)

The compat header provides typed-to-void bridges for the existing functions and no-op stubs for the new ones, keeping the 4.14 kernel patches and the v1.5.5 internal implementation intact.

---

## Security notes

- The `task_prctl` handler crowns the first process to call `prctl(0xDEADBEEF, 2, ...)` as manager. In practice this is always the manager app.
- The manager auto-grant in sucompat applies only to the crowned manager UID.
- "Hide SELinux modifications" in the manager UI cannot be enabled, it requires kernel 5.10+ structures (`selinux_hide` feature). Use Shamiko (Zygisk module) as an alternative.

---

## Credits

- [SukiSU-Ultra](https://github.com/SukiSU-Ultra/SukiSU-Ultra) -> kernel root solution
- [simonpunk/susfs4ksu](https://gitlab.com/simonpunk/susfs4ksu) -> SUSFS root hiding patches
- [YAAP](https://github.com/yaap) -> base kernel source
- [@sidex15, @maxsteeel, @rifsxd](https://kernelsu-next.github.io/webpage/pages/how-to-integrate-for-non-gki.html) -> non-GKI legacy integration guidance
