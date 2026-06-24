# AI Process-Terminate

A lightweight Windows utility that terminates desktop AI assistant processes
(Claude, Copilot, Perplexity, Codex, …) in one shot — no admin rights needed.

---

## Why

AI desktop apps (Claude, Copilot, Perplexity, Codex and similar) are known to
accumulate background processes over time. This can cause:

- **System slowdowns** — high CPU or RAM pressure from idle AI workers.
- **Frozen or unresponsive UI** — the app gets stuck but its processes keep
  running and holding resources.
- **GPU saturation** — local model runners keep the GPU busy even when the
  app window is closed.

Killing them from Task Manager is tedious when there are 5–15 scattered
processes per tool. AI Process-Terminate groups them by tool and removes the whole tree
in one keypress.

---

## What it does

1. **Scans** all running processes for names that partially match any known AI
   tool keyword.
2. **Groups** matches by tool and shows a compact dialog — one row per tool,
   with the number of running instances in parentheses, e.g. `claude (9)`.
3. **Lets you choose** which tools to terminate; the others are left untouched.
4. **Drains** all instances in a loop (up to 20 iterations, 200 ms apart) so
   that processes that respawn are caught and killed too.

Only processes owned by the **current user** are affected. System processes and
processes owned by other users are skipped gracefully.

---

## Requirements

- Windows 10 / 11 (x86 or x64)
- No installation, no admin rights, no external dependencies

---

## Total Commander integration

The release zip includes three TC-specific files:

| File | Purpose |
|---|---|
| `AIProcess-Terminate.bar` | Button bar definition (`TOTALCMD#BAR#DATA` format) |
| `pluginst.inf` | Auto-install descriptor — tells TC where to copy the files |
| `AIProcess-Terminate.ini` | Target keyword list (copied alongside the exe) |

### Auto-install (recommended)

1. Keep `AIProcess-Terminate.exe`, `AIProcess-Terminate.ini`,
   `AIProcess-Terminate.bar`, and `pluginst.inf` in the same folder.
2. In Total Commander open the folder and double-click
   `AIProcess-Terminate.bar` — TC recognises the `TOTALCMD#BAR#DATA` header
   and offers to import it directly, copying the exe and INI to
   `%COMMANDER_PATH%\Tools\` and adding the button to the bar.

Alternatively: **Configuration → Import plugin/button bar…** and point it at
`AIProcess-Terminate.bar`.

### Manual button

1. Copy `AIProcess-Terminate.exe` and `AIProcess-Terminate.ini` anywhere
   (e.g. `%COMMANDER_PATH%\Tools\`).
2. Right-click the TC button bar → **Change…** → enter:
   - **Command:** path to `AIProcess-Terminate.exe`
   - **Start path:** folder containing the exe
   - **Tooltip:** `AI Process-Terminate`

---

## Usage

### Interactive mode (default)

Double-click `AIProcess-Terminate.exe` or run it from a shell with no arguments.

A dialog appears listing every detected AI tool and how many processes it has
running:

```
┌─ AI Process-Terminate -- Select tools to terminate ──────────────────┐
│  Tool                                                      │
│  ☑ claude (9)                                             │
│  ☑ copilot (3)                                            │
│  ☐ perplexity (1)                                         │
└────────────────────────────────────────────────────────────┘
│  Up/Dn Navigate   SPACE Toggle   ENTER Confirm   ESC Abort │
│                        [Terminate Selected]  [Abort]       │
└────────────────────────────────────────────────────────────┘
```

**Keyboard controls:**

| Key         | Action                              |
|-------------|-------------------------------------|
| `↑` / `↓`  | Move focus between tools            |
| `Space`     | Toggle checkbox (select / deselect) |
| `Enter`     | Confirm and terminate checked tools |
| `Esc`       | Abort — nothing is killed           |

Unchecked tools are **never touched**.  
Closing the dialog with the X button also aborts silently.

### Automatic mode

```
AIProcess-Terminate.exe -auto
```

or equivalently:

```
AIProcess-Terminate.exe /auto
```

Skips the dialog entirely, kills **all** detected AI tool processes, and exits.
No UI is shown. Suitable for scheduled tasks, startup scripts, or hotkey
launchers.

If no AI processes are running, the program exits immediately with code `0`
and no message.

---

## Command-line reference

| Argument      | Effect                                                  |
|---------------|---------------------------------------------------------|
| *(none)*      | Show interactive selection dialog                       |
| `-auto`       | Kill all detected tools silently, no dialog             |
| `/auto`       | Same as `-auto` (Windows-style slash accepted)          |

---

## Exit codes

| Code | Meaning                                                  |
|------|----------------------------------------------------------|
| `0`  | Success, or user aborted, or nothing was running         |
| `1`  | At least one process could not be terminated             |

---

## Extending the tool list

Edit `AIProcess-Terminate.ini` (placed next to the `.exe`) — no recompile needed:

```ini
[Targets]
keywords = claude, copilot, perplexity, codex, gemini, manus, antigravity
```

Add or remove comma-separated keywords. Matching is **partial and
case-insensitive**, so `gemini` matches `Gemini.exe`, `gemini-helper.exe`,
`com.google.gemini.exe`, etc.

**Currently targeted tools:**
`claude`, `copilot`, `perplexity`, `codex`, `gemini`, `manus`, `antigravity`

If the INI file is missing or the `keywords` key is absent, the built-in
default list above is used as a fallback.

## Building from source

Prerequisites: Visual Studio 2022+ with the **Desktop development with C++**
workload installed (MSVC v143 toolchain).

Just run:

```bat
build.bat
```

If no VS environment is active, the script auto-detects the installation via
`vswhere.exe` and bootstraps the x64 toolchain by default. To build for x86
instead, source `vcvars32.bat` (or any `vcvarsall.bat` variant) before running
`build.bat` — the script will use whatever architecture is already set up.

Output: `AIProcess-Terminate.exe` in the same folder.

The build script compiles `resource.rc` with `rc.exe`, which bundles:

| Resource | Details |
|---|---|
| Application icon | `AIProcess-Terminate.ico` (7 sizes, 16–256 px) |
| Manifest | `AIProcess-Terminate.manifest` — `asInvoker` UAC, Common Controls v6, Per-Monitor DPI v2, Windows 10/11 compatibility |
| Version info | `VERSIONINFO` block — file/product version, description, original filename |

The C++ source is then compiled with `/W4 /WX /utf-8` (warnings as errors, UTF-8 source) and linked
against `psapi.lib`, `comctl32.lib`, and `user32.lib`. No third-party
libraries are required.

---

## License

Do whatever you want with it.
