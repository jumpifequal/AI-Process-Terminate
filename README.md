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

- Windows 10 / 11 x64
- No installation, no admin rights, no external dependencies

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

Open `AIProcess-Terminate.cpp` and find the section marked `// EXTEND HERE`:

```cpp
static const std::vector<std::wstring> TARGET_NAMES = {
    L"claude",
    L"copilot",
    L"perplexity",
    L"codex",
    L"gemini",
    L"manus",
    L"antigravity",
    // EXTEND HERE
};
```

Add any keyword that appears in the executable name of the tool you want to
target. Matching is **partial and case-insensitive**, so `L"gemini"` would
match `Gemini.exe`, `gemini-helper.exe`, `com.google.gemini.exe`, etc.

**Currently targeted tools:**
`claude`, `copilot`, `perplexity`, `codex`, `gemini`, `manus`, `antigravity`

Then rebuild with `build.bat`.

---

## Building from source

Prerequisites: Visual Studio 2022+ with the **Desktop development with C++**
workload installed (MSVC v143 toolchain, x64).

Open a **Developer Command Prompt for VS 2022** (or any prompt where
`vcvarsall.bat x64` has already been sourced) and run:

```bat
build.bat
```

Output: `AIProcess-Terminate.exe` in the same folder.

The build script compiles `resource.rc` with `rc.exe` to embed the application
icon, then compiles the C++ source with `/W4 /WX` (all warnings treated as
errors) and links `psapi.lib`, `comctl32.lib`, and `user32.lib`. No
third-party libraries are required.

---

## License

Do whatever you want with it.
