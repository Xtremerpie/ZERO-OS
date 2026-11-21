# Application Architecture

**Status: design only (6.0.0). Nothing exists in 0.0.0.**

- **Native format:** ELF executables (ELF loader arrives in 2.0.0) packaged in a `.zpk` archive
  with a manifest.
- **Manifest** (validated schema): id, version, publisher, declared permissions (files, network,
  microphone, AI, …), dependencies with version ranges, entry point, minimum ZERO OS version.
- **Package manager:** install, uninstall, update, dependency resolution, repository support;
  transactions are atomic and recorded (shares machinery with ZERO Config, 5.0.0).
- **Trust:** every package is signature-verified against a publisher key before any code runs;
  the permission list is shown and requires explicit approval; unverified packages are refused.
- **Sandbox:** applications run with only their granted permissions (process isolation from 1.0.0
  plus sandbox policy from 8.0.0).
- **ZERO Store:** discover, inspect permissions, install/update/uninstall, verify publisher.
- **Compatibility:** no Windows application compatibility is claimed or planned.
