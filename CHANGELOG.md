# Changelog

---

## [2.0.1]

* fix: wait briefly after successful process termination so the drain loop does not immediately rescan processes that Windows is still tearing down
* fix: clarify the max-iteration error when matching processes keep respawning or cannot finish terminating
* fix: compile sources as UTF-8 so Unicode text in dialogs and documentation strings is preserved in MSVC builds

---

## [2.0.0]

* add: target process list moved to `AIProcess-Terminate.ini` — edit keywords without recompiling; falls back to built-in defaults if file is absent
* add: application icon embedded in the executable (7 sizes: 16 – 256 px)
* add: Windows manifest — `asInvoker` UAC, Common Controls v6, Per-Monitor DPI v2, Windows 10/11 compatibility
* add: `VERSIONINFO` resource — file/product version and description visible in Explorer → Properties → Details
* add: resizable dialog — all controls reflow on resize; minimum size enforced at 400×280
* add: Per-Monitor DPI v2 support — layout constants and initial window size scale with monitor DPI; adapts on DPI change (`WM_DPICHANGED`)
* add: Total Commander integration — `pluginst.inf` + `AIProcess-Terminate.bar` for one-click button bar install
* change: default dialog size increased from 520×320 to 640×460 (at 96 DPI)
* change: build script auto-detects Visual Studio via `vswhere.exe`; honours any pre-sourced `vcvarsall` variant

---

## [1.0.0]

* add: scan and group running processes by AI tool keyword (`claude`, `copilot`, `perplexity`, `codex`, `gemini`, `manus`, `antigravity`)
* add: interactive selection dialog with per-tool instance count and keyboard navigation (↑↓ Space Enter Esc)
* add: silent `-auto` / `/auto` mode — kills all detected tools without UI
* add: multi-instance drain loop — re-scans up to 20 times to catch processes that respawn
* add: current-user-only scope — system and other-user processes are skipped gracefully
