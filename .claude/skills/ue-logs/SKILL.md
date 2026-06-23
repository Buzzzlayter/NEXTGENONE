---
name: ue-logs
description: Read the running Unreal Editor's output log as text — errors, warnings, asserts, Live Coding compile results, or any category — via the in-editor MCP server. Use when the user asks to check the log / output log / warnings / "what went wrong", to confirm something happened at runtime, to read PIE output, or to see a Live Coding compile result. Requires the editor to be running (MCP up); if it's closed, read Saved/Logs/NEXTGENONE.log directly instead.
---

# /ue-logs — read the editor output log as text

The editor's output log is reachable as text through the `EditorToolset.LogsToolset` toolset
on the in-editor MCP server (see the `ue` skill for connection/preflight). Four tools:
`GetLogEntries`, `GetLogCategories`, `GetVerbosity`, `SetVerbosity`.

## The one gotcha that breaks every first call

`category` defaults to `"LogsToolset"`, which is **not a real category** — leaving it out
raises `Log category 'LogsToolset' not found`. **Always pass `category` explicitly**; use
`""` to search across all categories.

## Read entries

`call_tool` → toolset `EditorToolset.LogsToolset`, tool `GetLogEntries` (SHORT name, no prefix):

```json
// errors + warnings + asserts, everything, last 50
{ "category": "", "pattern": "(?i)error|warning|fatal|assert|ensure", "maxEntries": 50 }

// one category, no filter
{ "category": "LogTemp", "pattern": "", "maxEntries": 100 }

// Live Coding compile result (see also the ue-build skill)
{ "category": "LogLiveCoding", "pattern": "", "maxEntries": 30 }

// PIE / gameplay errors only
{ "category": "", "pattern": "(?i): error|: warning", "maxEntries": 80 }
```

- `pattern` is a regular expression; prefix `(?i)` for case-insensitive.
- `maxEntries` is taken from the **end** of the log (most recent); `0` = no limit.
- Entries come back as full log lines with timestamps and category, in chronological order.

## Find categories / change verbosity

```json
// list categories whose name contains a substring
GetLogCategories { "filter": "Live" }     // -> ["LogLiveCoding", ...]

// raise detail before reproducing an issue, then read again
GetVerbosity { "category": "LogTemp" }
SetVerbosity { "category": "LogTemp", "verbosity": "Verbose" }   // NoLogging|Fatal|Error|Warning|Display|Log|Verbose|VeryVerbose
```

## When the editor is closed (MCP down)

The MCP server only answers while the editor runs. With it closed, read the previous session's
log file directly: `Saved/Logs/NEXTGENONE.log` (use Read/Grep). Older sessions are kept as
`Saved/Logs/NEXTGENONE-backup-*.log`.

## Asserts / crashes

`check()` / `ensure()` / fatal asserts appear as `Fatal`/`Error` lines (often
`Assertion failed:` or category `LogOutputDevice`/`LogWindows`). The pattern
`(?i)assert|fatal|ensure condition failed` catches them. A hard crash may close the editor
(MCP goes down) — then fall back to the on-disk log file above.
