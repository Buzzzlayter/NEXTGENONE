---
name: ue-build
description: Verify that this UE C++ project actually compiles and surface real compiler errors/warnings as text — so C++ changes are checked, not hallucinated. Use after editing any .h/.cpp/.Build.cs/.Target.cs, when the user asks to "build/compile/check the C++", or before relying on a new C++ API. Picks the right path automatically: full offline UBT build when the editor is closed, or Live Coding (Ctrl+Alt+F11) + reading LogLiveCoding when the editor is open.
---

# /ue-build — compile UE C++ and read the result as text

An agent that writes C++ without seeing the compiler output is guessing the API.
This skill makes the build result **text you can read**. Two scripts live next to this
file; run them from the **project root** with Windows PowerShell.

## Decide the path first: is the editor running?

```powershell
[bool](Get-Process UnrealEditor -ErrorAction SilentlyContinue)
```

- **Editor CLOSED → full offline build (authoritative).** Compiles *and* links; reports
  every compiler error/warning. This is the source of truth.
- **Editor OPEN → Live Coding.** UBT refuses to build while Live Coding is active
  (`Unable to build while Live Coding is active`), so hot-patch instead and read the result
  from the log. Closing the editor also kills the MCP connection — keep it open unless you
  deliberately want a full offline build.

## Path A — full offline build (editor closed)

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .Codex/skills/ue-build/Verify-Build.ps1
```
Flags: `-Game` (game target instead of editor), `-Config Debug|DebugGame|Development|Shipping`.

Reads back: a compact `RESULT` plus `Errors`/`Warnings` lines in MSVC form
`File.cpp(42): error C2065: ...`. Full UBT output is saved to `Saved/Logs/AgentBuild.log`.

**Exit codes (branch on these):**
- `0` — PASS. Code compiles. (A link-only lock with the editor open also returns 0: the
  compile succeeded, only the DLL write was blocked.)
- `3` — BLOCKED: Live Coding is active / editor open, **nothing was compiled**. Not a code
  error — either close the editor and re-run, or use Path B.
- anything else — real build failure; read the `Errors` it printed.

## Path B — Live Coding hot-compile (editor open)

1. Trigger the compile (best-effort GUI automation — see caveat):
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .Codex/skills/ue-build/Trigger-LiveCoding.ps1
   ```
   This activates the editor window and sends **Ctrl+Alt+F11**. If the user prefers, they can
   press Ctrl+Alt+F11 themselves — the result-reading below is identical.
2. Wait a few seconds (Live Coding compiles asynchronously), then read the result as text via
   the `ue` skill / MCP `EditorToolset.LogsToolset.GetLogEntries`:
   ```json
   { "category": "LogLiveCoding", "pattern": "", "maxEntries": 30 }
   { "category": "", "pattern": "(?i)error C|error LNK|warning C", "maxEntries": 50 }
   ```
   A clean compile logs a success line under `LogLiveCoding`; failures show the usual
   `error Cxxxx` / `error LNKxxxx` lines.

> **Caveat:** `Trigger-LiveCoding.ps1` drives the GUI via SendKeys, so it needs the editor
> window on the same desktop and briefly steals focus. Windows can refuse a foreground switch
> from a background process; if nothing compiled, fall back to a manual Ctrl+Alt+F11. Use
> `-Diagnose` to print the target window without sending keys.

## Gotchas

- These scripts are **self-contained**: they find the `.uproject` beside the project root and
  resolve the engine from `EngineAssociation` via the registry — nothing is hard-coded.
- A full relink **cannot** happen while the editor holds the module DLL — that is expected, not
  a code error. Path A reports it as `LINK BLOCKED` / exit `0`.
- For editor logs, warnings and asserts (not compilation), use the **`ue-logs`** skill.
