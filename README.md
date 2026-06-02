# OnePlus 7 Pro (guacamole) — YAAP 16 Kernel with SukiSU-Ultra

Linux 4.14.336 for the OnePlus 7 Pro (SM8150 / `guacamole`) running [YAAP 16](https://github.com/yaap), with [SukiSU-Ultra](https://github.com/SukiSU-Ultra/SukiSU-Ultra) (`builtin` branch, non-GKI) fully integrated.

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

- **SukiSU-Ultra** (`builtin` branch, v4.1.3) manually integrated via non-GKI kernel hooks
- All six hook sites patched: `fs/exec.c`, `fs/open.c`, `fs/read_write.c`, `fs/stat.c`, `kernel/reboot.c`, `drivers/input/input.c`
- Built-in cert allowlist accepts the official SukiSU-Ultra manager, KernelSU-Next manager, and several other common forks — no custom signing required
- KPM disabled (requires Linux 5.0+ APIs not present in 4.14)

---

## Build requirements

| Tool | Where to get |
|---|---|
| Clang `r547379` (clang 20) | [android-clang](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/) — place at `../android-clang/clang-r547379/` relative to this repo |
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

Download the official SukiSU-Ultra manager APK from the [SukiSU-Ultra releases page](https://github.com/SukiSU-Ultra/SukiSU-Ultra/releases). The kernel's cert allowlist already includes SukiSU-Ultra's official signing certificate — no custom build needed.

The kernel also accepts these other manager certs out of the box:

| Manager | Notes |
|---|---|
| SukiSU-Ultra (official) | Primary target |
| KernelSU-Next (official) | `79e590...` / size `0x3e6` |
| KernelSU (official) | `c371061...` / size `0x033b` |
| RKSU, MKSU, KowSU | Additional forks |

---

## Repacking the boot image

You need the original `boot.img` from your device:

```bash
adb pull /dev/block/by-name/boot boot.img
```

Extract the ramdisk and repack:

```python
# Extract ramdisk
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
| `fs/stat.c` | Added `ksu_handle_stat()` in `vfs_statx()` |
| `kernel/reboot.c` | Added `ksu_handle_sys_reboot()` at the top of `SYSCALL_DEFINE4(reboot, ...)` |
| `drivers/input/input.c` | Added `ksu_handle_input_handle_event()` in `input_handle_event()` |
| `drivers/Kconfig` | Added `source "drivers/kernelsu/Kconfig"` |
| `drivers/Makefile` | Added `obj-$(CONFIG_KSU) += kernelsu/` |

### Build fixes

| File | What changed |
|---|---|
| `arch/arm64/kernel/vdso32/Makefile` | `LD_COMPAT ?= $(LD)` instead of `$(CROSS_COMPILE_COMPAT)ld` — uses `arm-linux-gnueabi-ld` for vdso32 instead of the host `ld` which has no ARM target |
| `lib/Kconfig` | Added `select ZSTD_COMMON` to `ZSTD_COMPRESS` and `ZSTD_DECOMPRESS` — fixes undefined `HUF_readStats` / `FSE_readNCount` at link time |

### defconfig (`arch/arm64/configs/neptune_defconfig`)

| Config | Reason |
|---|---|
| `CONFIG_KSU=y` | SukiSU-Ultra core |
| `CONFIG_KSU_DEBUG=y` | Debug logging — remove for release builds |
| `CONFIG_KALLSYMS_ALL=y` | Required by SukiSU-Ultra for non-GKI builds |
| `# CONFIG_KPM is not set` | KPM uses Linux 5.0 APIs (`access_ok` 2-arg, etc.) not available on 4.14 |
| `# CONFIG_KSU_SUSFS is not set` | SUSFS requires additional kernel patches not included here |
| `CONFIG_CPU_INPUT_BOOST=y` | `kernel/fork.c` and `kernel/sched/core.c` reference symbols defined only in `cpu_input_boost.c` |

### SukiSU-Ultra internal patches (`KernelSU/kernel/`)

**`KernelSU/kernel/runtime/ksud.c`** — throne tracking never ran on fresh installs

`on_post_fs_data()` fires when Zygote starts. In the built-in (non-LKM) path, `track_throne()` — the scan that finds and records the manager APK UID — was never called: the late-loaded path called it explicitly, the built-in path didn't, and `ksu_cred` had no proper SELinux context. Fixed by calling `apply_kernelsu_rules()` + `cache_sid()` + `setup_ksu_cred()` then `track_throne()` at the start of `on_post_fs_data()`, plus a 3-second delayed retry for when `packages.list` is available from PMS.

Also guarded all `ksu_selinux_hide_*` calls with `LINUX_VERSION_CODE >= 5.10` (they were unconditional in the `builtin` branch — a bug causing implicit-declaration errors on 4.14).

**`KernelSU/kernel/hook/lsm_hook.c`** — manager detection circular dependency

Added a `task_prctl` LSM hook for `prctl(0xDEADBEEF, 2, ...)`. On first call, crowns the caller's UID as manager and installs the `[ksu_driver]` fd directly into the calling process, breaking the dependency where the ioctl fd couldn't be installed without throne tracking having already found the APK.

**`KernelSU/kernel/feature/sucompat.c`** — manager couldn't bootstrap

Added `|| is_uid_manager(current_uid().val)` to `__is_su_allowed()` so the manager can execute `su` before it has explicitly granted itself root. On a fresh install the allowlist is empty; without this the manager deadlocks trying to get root to set up root.

**`KernelSU/kernel/manager/apk_sign.c`** — custom manager cert

Added a custom signing key entry (`0x306` / `363a3e...`) alongside the upstream cert allowlist, allowing a locally-built manager APK to be used alongside the official releases.

**`KernelSU/kernel/manager/pkg_observer.c`** — missed packages.list writes

Widened `MASK_SYSTEM` with `FS_MODIFY | FS_CLOSE_WRITE` so direct writes to `packages.list` (not just atomic renames) trigger a throne re-scan.

**`KernelSU/kernel/sulog/event.c`** — pointer/value mismatch

`user_arg_null_ptr()` returns `struct user_arg_ptr *` but `ksu_sulog_capture` expects `struct user_arg_ptr` by value. Fixed with `*user_arg_null_ptr()`.

---

## Security notes

- The `task_prctl` handler crowns the **first process** to call `prctl(0xDEADBEEF, 2, ...)` as the manager. In practice this is always the manager app, but cert verification via `track_throne()` will eventually override it once `packages.list` is available.
- The manager auto-grant in sucompat only applies to the crowned manager UID — no other app is affected.

---

## Credits

- [SukiSU-Ultra](https://github.com/SukiSU-Ultra/SukiSU-Ultra) — kernel root solution
- [YAAP](https://github.com/yaap) — base kernel source
- [@sidex15, @maxsteeel, @rifsxd](https://kernelsu-next.github.io/webpage/pages/how-to-integrate-for-non-gki.html) — non-GKI legacy integration guidance
