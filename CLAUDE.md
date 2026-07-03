# Brewing Station — Claude Code Context

Monorepo for a DIY automated beer brewing controller, across several firmware/hardware generations. See [README.md](README.md) for the human-facing overview.

## Layout

- `brewingstation/` — V1 firmware (single `.ino`, byte-identical across its two hardware variants: breadboard and a custom PCB, see `hardware/breadboard/` and `hardware/pcb/`). Still functional, in light maintenance — bug fixes and doc updates are fine, but don't restructure it or backport v3 patterns without being asked.
- `brewingstation3/` — active development. Has its own [brewingstation3/CLAUDE.md](brewingstation3/CLAUDE.md) with detailed hardware/KiCad/backlog context — read that before working in this folder.
- `Brautomat32-main/`, `MQTTDevice2/`, `MQTTDevice32pIO-main/` — vendored copies of other people's repos kept locally for reference/copying code. Not tracked in git, not part of this project's history. Don't edit them as if they were ours.

## Conventions that apply everywhere

- **Secrets:** every project's `config.h` is gitignored; `config_example.h` (committed) is the template. Never put real WiFi/MQTT/OTA credentials in a tracked file.
- **Build:** PlatformIO (`pio run -e <env>`) is the primary toolchain for every project. `brewingstation` also opens directly in the Arduino IDE (its `.ino` matches the folder name); `brewingstation3` is PlatformIO-only.
- **License:** MIT (see [LICENSE](LICENSE)). Attribute InnuendoPi's [MQTTDevice2](https://github.com/InnuendoPi/MQTTDevice2) when touching anything related to the GGM IDS2 induction cooker protocol — every generation's serial-protocol code originates there.
