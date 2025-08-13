# Pixie Recommends Installing Your Distro’s Linux Kernel Headers

Pixie uses eBPF programs; some are compiled **at runtime** (via BCC/LLVM) against the node’s running kernel. That compilation needs the kernel’s C headers that match the exact kernel build (including vendor backports). Mismatched or missing headers can lead to compile failures or silently incorrect offsets in tracers.

---

## TL;DR

- **If Pixie deploys and its tracers start successfully, you can ignore the kernel-headers warning.** It’s informational.
- **If you hit *any* Pixie issues or suspect a bug, first install your distro’s kernel headers on every node and re-test.** This resolves a large class of tracer initialization problems on vendor-patched kernels.
- **Note:** Some minimal/immutable distros don’t ship a headers package (notably **Google Container-Optimized OS / COS**). See guidance below.

> **Screenshot of the warning in `px`**
>
> Replace the URL below with the image from PR **#2250**.
>
> `![Missing kernel headers — informational warning](INSERT_IMAGE_URL_FROM_PR_2250)`

---

## Why headers matter

- **Runtime compilation (BCC):** Pixie compiles certain eBPF programs on the node. BCC needs kernel headers to resolve structure layouts, constants, and feature guards accurately. Without a match, compilation may fail or produce incorrect code.
- **Vendor patches/backports:** Distro kernels often diverge from upstream. Only the **distro’s header package** reflects those exact changes; generic or prepackaged headers are best-effort and can be wrong.

---

## What you’ll see when headers are missing

- A **warning** during deploy/validation that headers weren’t detected.
- In agent logs (e.g., `vizier-pem`), messages about missing `/lib/modules/$(uname -r)/{build,source}` and attempts to fall back to bundled headers.
- Tracers that rely on BCC may fail to initialize (socket/HTTP/DNS tracing most commonly).

---

## Install instructions (per distro)

> Ensure the installed header version **matches `uname -r`**. If a kernel update is pulled in, **reboot** so the running kernel matches the headers.

- **Debian/Ubuntu**
  ```bash
  sudo apt-get update
  sudo apt-get install -y linux-headers-$(uname -r)
  ```

- **RHEL / Rocky / Alma / Fedora**
  ```bash
	# DNF-based
	sudo dnf install -y kernel-devel-$(uname -r)
	# YUM-based
	sudo yum install -y kernel-devel-$(uname -r)
	# (Some environments also need: kernel-headers-$(uname -r))
  ```

- **Amazon Linux**
  ```bash
	 AL2
	sudo yum install -y kernel-devel-$(uname -r)

	# AL2023
	sudo dnf install -y kernel-devel
	# If this pulled a newer kernel, reboot so uname -r matches.
  ```

- **openSUSE**
  ```bash
  sudo zypper install -y kernel-default-devel
  ```

## Distros without kernel-headers packages (notable: Google COS)

Some immutable/minimal distros **don’t provide a headers package or a host package manager**:

- **Google Container-Optimized OS (COS):** Root filesystem is read-only; there’s no `apt`/`dnf` on the host. COS’s `toolbox` is for ephemeral debugging and not for installing kernel headers on the node.  
  **Options:**
  - Use **Ubuntu** (or other header-capable) node images on **GKE Standard** when you need BCC runtime compilation.
  - For **Autopilot** (COS-based), consider running Pixie on **Standard** for debugging, or use a custom image/variant that includes headers where feasible.
  - Pixie will try **prepackaged headers** as a fallback, but these are not guaranteed to match vendor-patched kernels.
  - If available in your environment, loading **in-kernel headers** via `kheaders` (`/sys/kernel/kheaders.tar.xz`) can help, but support varies by vendor.

Other minimal/immutable OSes (e.g., Fedora CoreOS, Bottlerocket, MicroOS) have similar constraints—prefer images/variants that include headers or build them into your node image pipeline.

---

## Troubleshooting checklist

1. **Match versions:** `uname -r` must match the headers you installed.
2. **Reboot after updates:** If the package manager updated the kernel, reboot nodes.
3. **Check presence:** Ensure `/lib/modules/$(uname -r)/build` and `.../include` exist.
4. **Review logs:** Look at `vizier-pem` logs for BCC compile/load failures.
5. **Try headers first:** If you see tracer init failures, install headers and re-test before deeper debugging.

---

## FAQ

**Is BTF/CO-RE enough to avoid headers?**  
Not universally. Pixie still uses BCC in parts of the data path, which needs headers at runtime. BTF helps CO-RE workflows, but does not remove the need for matching headers where BCC is used.

**What breaks if I skip headers?**  
Most commonly, the **socket/HTTP/DNS tracers** fail to initialize. You might still see some metrics, but protocol-level visibility will be limited.

**Why prefer distro headers over Pixie’s packaged headers?**  
Because **distro kernels are not vanilla**—they include backports and config deltas. Distro header packages are the authoritative match; bundled headers are a convenience, not a guarantee.

---

## Related references

- Pixie PR: **#2250** — CLI warning message and docs link  
  https://github.com/pixie-io/pixie/pull/2250
- Pixie Issue: **#2051** — Recommend installing distro headers; examples across distros  
  https://github.com/pixie-io/pixie/issues/2051
