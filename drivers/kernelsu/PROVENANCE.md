# KernelSU (XXKSU) — Import Provenance

This directory imports the **kernel/** (and repo-root **uapi/**) source of the
modern KernelSU driver so it can be built into the Android Common Kernel image as
an optional, **disabled-by-default** Kconfig driver (`CONFIG_KSU`).

## Imported revision

- Repository: `KOWX712/KernelSU` (the community "XXKSU" fork)
- Commit: `72bd84056f905575c255445053b8a81d298312e8`
  - author: `KOWX712 <leecc0503@gmail.com>`
  - date: Wed Jul 29 11:28:05 2026 +0800
  - subject: `manager: translation squash`
  - branch: `master`
- Imported trees (byte-for-byte from that commit):
  - `kernel/` -> `drivers/kernelsu/kernel/`
  - `uapi/` -> `drivers/kernelsu/uapi/`

## Upstream lineage

- `tiann/KernelSU` — upstream KernelSU. `kernel/` base tracked to
  `tiann@953b403ab5ea2c1d866f72f6a7e67ce9224b8932`
  (kernel tree `b551865e3bce7b8eb67ec2cd07a19ba4826e945e3`). This is the
Merge-base of the imported tree with upstream `tiann/master`.
- `backslashxx/KernelSU` — fork of `tiann`; historically carried a **legacy**
  unity-build driver (`ksu.o`, syscall-table tampering) for older kernels, plus
  the raw pieces later folded into the modern tree (e.g. AVC log spoofing,
  a "tiny su-log" ring buffer).
- `KOWK712/KernelSU` ("XXKSU") — fork of `backslashxx`; restored the **modern**
  hook architecture (LSM / syscall-hook-manager / supercall / selinux /
  sulog) which mirrors `tiann/master`'s layout, while keeping the fork's
  sulog + AVC-spoof additions.

## Tree delta vs upstream `tiann` base

Relative to `tiann@9453b403a` (:kernel), the imported `kernel/` differs in
exactly **13 files** (verified with `git diff --name-status`):

- Modified (11)
  - `kernel/Kbuild`
  - `kernel/Makefile` (module build; not imported — see "Excluded")
  - `kernel/setup.sh` (integration script; not imported)
  - `kernel/feature/sucompat.c`
  - `kernel/hook/syscall_hook_manager.c/.h`
  - `kernel/policy/allowlist.c`
  - `kernel/runtime/boot_event.c`
  - `kernel/supercall/dispatch.c`
  - `kernel/supercall/supercall.c/.h`
- Added (2) — both are fork-added files (see authorship):
  - `kernel/extras.c`
  - `kernel/tiny_sulog.c`

## Authorship

- The unmodified (byte-identical) bulk of the driver matches upstream
  `tiann/master` (commit `953b403a`), retaining tiann authorship.
- Within the imported fork's history, the **last** author recorded for the 13
  delta files — including `extras.c` and `tiny_sulog.c` — is `backslashxx`
  (e.g. commits `18d560a9 kernel: extras: avc log spoofing`, `f4923529
  kernel: tiny_sulog: basic ringbuffer`). i.e. the fork-specific delta is
  predominately attributable to the `backslashxx` fork line.
- The `uapi/` delta vs `tiann` is a single file: `uapi/feature.h` (modified).
- This CloudFox integration does **not** claim authorship of the imported
  kernel code; it preserves the upstream/fork attribution above.

## Integration-only changes made here (deliberate, minimal)

1. `drivers/kernelsu/kernel/Kconfig`:
   - `default y` -> `default n`. Single behavioural edit. Upstream ships
     `default y`; left as-is a default build (`gki_defconfig`, no
     `CONFIG_KSU` line) would silently select `CONFIG_KSU=y`. We need the
     driver **off by default**, opt-in only. (`tristate` preserved; the module
     path is not exercised in this tree — LKM loads require the separate
     userspace `ksuinit` loader and are out of scope for the built-in
     integration.)
2. **`drivers/Kconfig`** (repository file): +`source
   "drivers/kernelsu/kernel/Kconfig"`.
3. **`drivers/Makefile`** (repository file): +`obj-$(CONFIG_KSU) +=
   kernelsu/kernel/`.
   - When `CONFIG_KSU` is unset (default), the `obj-` line is empty, so
     `drivers/kernelsu/kernel/` is never entered — nothing from KernelSU is
     compiled or linked.

No defconfig, device defconfig, fragment, DBGONFIG, or other configuration has
`CONFIG_KSU` set.

## Excluded & not imported

- Not required for the in-tree built-in compile: `kernel/Makefile`
  (external-module build), `kernel/setup.sh`, `kernel/build-all.sh`,
  `kernel/tools/`, `kernel/.vscode/`, `.clang-format`, `.clangd.example`,
  `.gitignore`.
- The `include/uapi` symlink (`-> ../../uapi`) is preserved; it resolves
  in-tree to `drivers/kernelsu/uapi/`.

## License

KernelSU kernel code is GPL-2.0. A copy of the original project license is at
`drivers/kernelsu/kernel/LICENSE`.