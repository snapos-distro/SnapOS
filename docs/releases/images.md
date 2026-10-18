The SnapOS images. They have no version: the universal image installs the newest SnapOS, and the manual one builds your system from the newest packs.

| Image | x86_64 | ARM64 |
| --- | --- | --- |
| **Universal** (net install) | `snapos-installer.iso` | `snapos-installer-aarch64.iso` |
| **Manual** (minimal) | `snapos-minimal.iso` | `snapos-minimal-aarch64.iso` |

**Universal.** The installer asks a few questions and installs the full SnapOS. With the internet up it downloads the newest SnapOS from [snapos-updates](https://github.com/snapos-distro/snapos-updates/releases/latest) and checks its checksum; without the internet it installs the copy on the stick. The review screen shows which one.

**Manual.** A terminal and no installer: you write your own system the NixOS way, in `/etc/snapos`, and add the SnapOS packs you want. The [handbook](https://snapos-distro.github.io/handbook/install.html) walks through every step.

SnapOS versions and what changed in each one are in [snapos-updates](https://github.com/snapos-distro/snapos-updates/releases).

Flash with balenaEtcher. Checksums in the `.sha256` files.
