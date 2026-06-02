# OnePlus 7 Pro (guacamole) — YAAP 16 Kernel with KernelSU-Next

Linux 4.14.336 for the OnePlus 7 Pro (SM8150 / `guacamole`) running [YAAP 16](https://github.com/yaap), with [KernelSU-Next](https://github.com/KernelSU-Next/KernelSU-Next) (legacy non-GKI) fully integrated.

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

- **KernelSU-Next** (`legacy` branch, v3.2.0-legacy) manually integrated via non-GKI kernel hooks
- All five required hook sites patched: `fs/exec.c`, `fs/open.c`, `fs/read_write.c`, `fs/stat.c`, `kernel/reboot.c`
- Several build and runtime fixes needed to make a clean 4.14 + Clang + ThinLTO build

---

## Build requirements

| Tool | Where to get |
|---|---|
| Clang `r547379` (clang 20) | [android-clang](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/) — place at `../android-clang/clang-r547379/` relative to this repo |
| `arm-linux-gnueabi-*` | `apt install gcc-arm-linux-gnueabi` |
| `aarch64-linux-gnu-*` | `apt install gcc-aarch64-linux-gnu` |
| Python 3, `mkbootimg` | `apt install python3 mkbootimg` |
| Java 17+ | Required only if building the manager APK |

---

## Building the kernel

```bash
./build.sh
```

Output: `arch/arm64/boot/Image.gz-dtb`

### Manager cert hash

By default `build.sh` embeds a hardcoded cert hash. To use the **official KernelSU-Next v3.2.0 manager** from the [releases page](https://github.com/KernelSU-Next/KernelSU-Next/releases) instead, override the hash at build time:

```bash
KSU_NEXT_MANAGER_SIZE=0x3e6 \
KSU_NEXT_MANAGER_HASH=79e590113c4c4c0c222978e413a5faa801666957b1212a328e46c00c69821bf7 \
./build.sh
```

To use your own signing key (needed for custom manager builds), see [Building the manager](#building-the-manager).

---

## Repacking the boot image

You need the original `boot.img` from your device:

```bash
adb pull /dev/block/by-name/boot boot.img
```

Then extract the ramdisk and repack:

```python
# Extract ramdisk (Python — adjust path as needed)
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

## Building the manager

The manager APK must be built from the [KernelSU-Next `dev` branch](https://github.com/KernelSU-Next/KernelSU-Next) with the `ksud` Rust daemon included. Pre-built releases from the KernelSU-Next releases page also work if you use their cert hash (see above).

### Custom build

Requirements: Android SDK (API 36), NDK 29, JDK 21, Rust stable ≥ 1.88.

```bash
# Clone dev branch (sparse — manager + uapi + userspace only)
git clone --filter=blob:none --sparse https://github.com/KernelSU-Next/KernelSU-Next.git ksunext-full
cd ksunext-full
git sparse-checkout set manager uapi userspace

# Build ksud Rust daemon
rustup target add aarch64-linux-android
cargo install cargo-ndk
cd userspace/ksud
ANDROID_NDK_HOME=/path/to/ndk cargo ndk -t arm64-v8a build --release
cp target/aarch64-linux-android/release/ksud \
   ../../manager/app/src/main/jniLibs/arm64-v8a/libksud.so
cd ../..

# Link the uapi headers for the JNI native code
ln -sf "$(pwd)/uapi" manager/app/src/main/cpp/uapi

# Build APK
echo "sdk.dir=/path/to/android-sdk" > manager/local.properties
cd manager
./gradlew assembleRelease
```

The unsigned APK is at `app/build/outputs/apk/release/KernelSU_Next_*-release.apk`.

Sign it with your own key:

```bash
keytool -genkeypair -alias ksu -keyalg RSA -keysize 2048 -validity 10000 \
  -keystore ksu.keystore -storepass YOUR_PASS -keypass YOUR_PASS \
  -dname "CN=KSU,O=KSU,C=US"

apksigner sign --ks ksu.keystore --ks-pass pass:YOUR_PASS \
  --ks-key-alias ksu --out KernelSU-signed.apk \
  app/build/outputs/apk/release/KernelSU_Next_*-release.apk
```

Extract the cert hash for `build.sh`:

```python
python3 - <<'EOF'
import struct, hashlib, sys
apk = "KernelSU-signed.apk"
with open(apk,'rb') as f: data=f.read()
eocd=data.rfind(b'\x50\x4b\x05\x06')
cd_off=struct.unpack_from('<I',data,eocd+16)[0]
m=data[:cd_off].rfind(b'APK Sig Block 42')
sb=struct.unpack_from('<Q',data,m-8)[0]
pos=m-8-(sb-24)
while pos<m-16:
    plen=struct.unpack_from('<Q',data,pos)[0]
    pid=struct.unpack_from('<I',data,pos+8)[0]
    if pid==0x7109871a:
        v2=data[pos+12:pos+8+plen]
        p=0
        for _ in range(3): struct.unpack_from('<I',v2,p); p+=4
        dl=struct.unpack_from('<I',v2,p)[0]; p+=4+dl
        struct.unpack_from('<I',v2,p)[0]; p+=4
        cl=struct.unpack_from('<I',v2,p)[0]; p+=4
        cert=v2[p:p+cl]
        print(f"KSU_NEXT_MANAGER_SIZE=0x{cl:x}")
        print(f"KSU_NEXT_MANAGER_HASH={hashlib.sha256(cert).hexdigest()}")
        sys.exit(0)
    pos+=8+plen
EOF
```

Rebuild the kernel passing those values to `build.sh`:

```bash
KSU_NEXT_MANAGER_SIZE=0x... KSU_NEXT_MANAGER_HASH=... ./build.sh
```

---

## Changes vs upstream YAAP kernel

### Kernel hook sites (`fs/`, `kernel/`, `drivers/`)

| File | What changed |
|---|---|
| `fs/exec.c` | Added `ksu_handle_execveat()` call at the top of `do_execveat_common()` |
| `fs/open.c` | Added `ksu_handle_faccessat()` call in `SYSCALL_DEFINE3(faccessat, ...)` |
| `fs/read_write.c` | Added `ksu_handle_vfs_read()` call in `vfs_read()` |
| `fs/stat.c` | Added `ksu_handle_stat()` call in `vfs_statx()` |
| `kernel/reboot.c` | Added `ksu_handle_sys_reboot()` call at the top of `SYSCALL_DEFINE4(reboot, ...)` — intercepts the magic-value reboot syscall the KSU daemon uses to install its driver fd |
| `drivers/input/input.c` | Added `ksu_handle_input_handle_event()` call in `input_handle_event()` |
| `drivers/Kconfig` | Added `source "drivers/kernelsu/Kconfig"` |
| `drivers/Makefile` | Added `obj-$(CONFIG_KSU) += kernelsu/` |

### Build fixes

| File | What changed |
|---|---|
| `arch/arm64/kernel/vdso32/Makefile` | Changed `LD_COMPAT ?= $(CROSS_COMPILE_COMPAT)ld` to `LD_COMPAT ?= $(LD)` so the vdso32 linker uses `arm-linux-gnueabi-ld` (already set by the `override LD` at the top of the file) instead of the host `ld` which has no ARM target support |
| `lib/Kconfig` | Added `select ZSTD_COMMON` to both `ZSTD_COMPRESS` and `ZSTD_DECOMPRESS` — the newer split zstd layout put shared code (`HUF_readStats`, `FSE_readNCount`, etc.) behind a separate `ZSTD_COMMON` config that nothing was selecting, causing undefined symbols at link |
| `scripts/module-lto.lds` | Replaced by KernelSU-Next's Kbuild with an updated LTO linker script that adds CFI-aware `.text` alignment and additional section patterns |

### KernelSU-Next Kbuild auto-patches

KernelSU-Next's `Kbuild` runs `sed` at compile time to backport several APIs this kernel version lacks. These are committed so the patches don't re-run on every clean build:

| File | What changed |
|---|---|
| `fs/internal.h` | Added `int path_umount(struct path *path, int flags);` declaration |
| `fs/namespace.c` | Added `path_umount()` implementation (needed by KSU's module unmounting) |
| `include/linux/seccomp.h` | Added `atomic_t filter_count` field to `struct seccomp` and `#include <linux/atomic.h>` |
| `security/selinux/include/objsec.h` | Added `selinux_inode()` and `selinux_cred()` inline helpers (backport from newer kernels) |
| `security/selinux/hooks.c` | Replaced direct `inode->i_security` casts with `selinux_inode(inode)` |
| `security/selinux/selinuxfs.c` | Replaced direct `inode->i_security` casts with `selinux_inode(inode)` |
| `security/selinux/xfrm.c` | Replaced `current_security()` with `selinux_cred(current_cred())` |

### defconfig (`arch/arm64/configs/neptune_defconfig`)

| Config added | Reason |
|---|---|
| `CONFIG_KSU=y` | KernelSU-Next |
| `CONFIG_KSU_DEBUG=y` | Debug logging — remove for release builds |
| `CONFIG_CPU_INPUT_BOOST=y` | `kernel/fork.c` and `kernel/sched/core.c` unconditionally reference symbols defined only in `drivers/cpufreq/cpu_input_boost.c` |

`arch/arm64/configs/vendor/sm8150-perf_defconfig` received the same `CONFIG_KSU=y` (and kprobe entries that are a no-op on this build — `CONFIG_MODULES` is not set so kprobes can't be enabled, but the entries are harmless).

### KernelSU-Next internal patches (`KernelSU-Next/kernel/`)

These patch files inside the vendored KernelSU-Next tree that were required to make the non-GKI built-in build actually work:

**`KernelSU-Next/kernel/runtime/boot_event.c`** — throne tracking never ran on fresh installs

`on_post_fs_data()` fires when Zygote starts (via the exec hook). In the normal built-in path (no LKM, no `ksud` installed), `track_throne()` — the scan that finds the manager APK and records its UID — was never called: the late-loaded path called it explicitly, the built-in path didn't, and `ksu_cred` hadn't been given a proper SELinux context yet. Fixed by calling `apply_kernelsu_rules()` + `cache_sid()` + `setup_ksu_cred()` then `track_throne()` at the start of `on_post_fs_data()`, and scheduling a 3-second delayed retry for when `packages.list` is available from PMS. Also widened `MASK_SYSTEM` in `pkg_observer.c` with `FS_MODIFY | FS_CLOSE_WRITE` so direct writes to `packages.list` (not just atomic renames) trigger a re-scan.

**`KernelSU-Next/kernel/hook/lsm_hooks.c`** — manager detection circular dependency

The manager's native library detects KSU via two paths: ioctl on the `[ksu_driver]` fd (primary), and `prctl(0xDEADBEEF, 2, ...)` (legacy fallback). The fd can only exist in the manager's process after the kernel has crowned the manager's UID — but crowning required `track_throne()` to have run — which requires `packages.list` to be available — which requires Android to have fully booted — by which point the manager has already checked and cached "KSU not found". Added a `task_prctl` LSM hook for `0xDEADBEEF` that (a) returns `KERNEL_SU_VERSION`, (b) crowns the caller's UID as manager on first call, and (c) installs the driver fd directly into the calling process, breaking the dependency cycle.

**`KernelSU-Next/kernel/feature/sucompat.c`** — manager couldn't bootstrap root shell

Two changes to `ksu_handle_execveat_sucompat`: added `|| is_uid_manager(current_uid().val)` to the allow-list check so the manager can execute `su` before it has explicitly granted itself root (on a fresh install the allow-list is empty); and added a check for `/data/adb/ksud` before redirecting `su` — if ksud isn't installed yet, falls back to `/system/bin/sh` with a root profile so the manager can bootstrap. After the manager's first run ksud installs itself and the fallback is never used again.

---

## Security notes

- The `task_prctl` handler crowns the **first process** to call `prctl(0xDEADBEEF, 2, ...)` as the manager. In practice this is always the KernelSU manager app (it starts before user apps), but it does not verify the APK cert the way `track_throne()` does. If cert-verified crowning matters to you, the delayed `track_throne()` call in `on_post_fs_data()` will eventually override the prctl-based crowning with a cert-verified one once `packages.list` is available.
- The manager auto-grant in sucompat only applies to the crowned manager UID — no other app benefits.
- The ksud fallback to `/system/bin/sh` is only active while `/data/adb/ksud` is absent (i.e., before the manager's first successful run). Once ksud installs itself the fallback path is never taken.

---

## Credits

- [KernelSU-Next](https://github.com/KernelSU-Next/KernelSU-Next) — kernel root solution
- [YAAP](https://github.com/yaap) — base kernel source
- [@sidex15, @maxsteeel, @rifsxd](https://kernelsu-next.github.io/webpage/pages/how-to-integrate-for-non-gki.html) — non-GKI legacy integration
