# Version Policy

ZERO OS has **exactly ten** official milestone versions. The machine-readable source of truth
is `configs/versions.json`; `tests/check_versions.py` (run by `make test`) fails if the list is
changed, reordered, extended, or disagrees with the kernel's `version.h`.

| # | Version | Name |
|---|---------|------|
| 1 | 0.0.0 | Genesis (complete) |
| 2 | 1.0.0 | Core Foundation (complete) |
| 3 | 2.0.0 | System Architecture (complete) |
| 4 | 3.0.0 | Hardware Foundation (complete) |
| 5 | 4.0.0 | Storage & Networking (complete) |
| 6 | 5.0.0 | ZERO Desktop (complete) |
| 7 | 6.0.0 | ZERO Ecosystem (complete) |
| 8 | 7.0.0 | ZERO Security (complete) |
| 9 | 8.0.0 | ZERO Intelligence (complete) |
| 10 | 9.0.0 | ZERO Ultimate (**current**, in progress) |

## Rules

- No eleventh version, no extra milestone numbers (no 0.1, 0.0.1, 1.1.0, …), no reordering, no skipping.
- A milestone is complete only when its acceptance criteria pass with recorded evidence.
- Internal development builds are identified by git hash/date (`ZERO_BUILD_ID`, from `git describe`), never by a new version.
- Each milestone must document: goals, features, architecture changes, tests, known limitations,
  security review, performance review, completion criteria, release notes. For 0.0.0 see
  `docs/MILESTONE_0.0.0.md`.
