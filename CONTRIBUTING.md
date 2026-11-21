# Contributing

1. Read `VERSION_POLICY.md`, `ARCHITECTURE.md` and `docs/CODING_CONVENTIONS.md`.
2. Work only on the current official milestone; do not start later-version features.
3. Inspect existing files before changing them; never destroy existing work; commit a checkpoint before architectural changes.
4. No placeholder implementations: a subsystem either works (with a test) or does not exist yet.
5. `make clean && make && make test` must pass before a change is considered done.
6. New third-party code or binaries: add to `third_party/` with pinned version, licence and SHA-256 in `THIRD_PARTY.md`.
7. Never claim hardware support, security, or performance without recorded evidence.
