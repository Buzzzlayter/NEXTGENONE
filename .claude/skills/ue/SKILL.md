---
name: ue
description: Drive the Unreal Engine editor of the current project through its in-editor MCP server (typically registered as `unreal-editor`). Use whenever the user wants to do something inside Unreal — inspect/place/move actors, edit blueprints, materials, Niagara, Sequencer, PCG, UMG, run PIE, read the editor log, etc. Handles the connection preflight and the meta-tool discovery loop.
---

# /ue — drive Unreal via the in-editor MCP server

The Unreal project in the current working directory hosts an MCP server **inside the
running editor** at `http://127.0.0.1:8000/mcp`. In Claude Code it is registered as an
HTTP-transport server — **typically `unreal-editor`**, so the meta-tools appear as
`mcp__unreal-editor__*`. If `claude mcp list` shows a different name, substitute it
everywhere `unreal-editor` appears below.

The server uses a **meta-tool** design: only three tools are exposed —
`mcp__unreal-editor__list_toolsets`, `mcp__unreal-editor__describe_toolset`,
`mcp__unreal-editor__call_tool` — and the real Unreal tools live inside *toolsets*
reached via `call_tool`.

## 1. Preflight — is the editor up and the server connected?

```bash
claude mcp list        # expect e.g.:  unreal-editor: http://127.0.0.1:8000/mcp (HTTP) - ✔ Connected
```

- **If it's Connected** → go to step 2.
- **If it's missing/disconnected**, the editor isn't running OR it started after this
  Claude session. The MCP client connects only at **session start**, so:
  1. Launch the editor for *this* project — don't hardcode anything; find the `.uproject`
     in the working dir and resolve the engine from its `EngineAssociation`:
     ```powershell
     $uproject = (Get-ChildItem -Path . -Filter *.uproject | Select-Object -First 1)
     $assoc = (Get-Content $uproject.FullName -Raw | ConvertFrom-Json).EngineAssociation
     $root = $null
     if ($assoc -match '^\{?[0-9A-Fa-f-]{36}\}?$') {
         # source/custom build: GUID -> path under …\Unreal Engine\Builds
         foreach ($hive in 'HKCU:','HKLM:') {
             $b = Get-ItemProperty "$hive\SOFTWARE\Epic Games\Unreal Engine\Builds" -ErrorAction SilentlyContinue
             if ($b) { $p = $b.PSObject.Properties | Where-Object { $_.Name.Trim('{}') -eq $assoc.Trim('{}') } | Select-Object -First 1; if ($p) { $root = $p.Value; break } }
         }
     } else {
         # launcher-installed: version (e.g. "5.8") -> InstalledDirectory
         foreach ($key in "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$assoc", "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$assoc") {
             $d = (Get-ItemProperty $key -ErrorAction SilentlyContinue).InstalledDirectory
             if ($d) { $root = $d; break }
         }
     }
     if (-not $root) { throw "Couldn't resolve engine for EngineAssociation '$assoc' — pass the UnrealEditor.exe path manually." }
     Start-Process (Join-Path $root "Engine\Binaries\Win64\UnrealEditor.exe") $uproject.FullName
     ```
     (`EngineAssociation` of `""` means the engine sits beside the project — launch the
     `UnrealEditor.exe` from that local engine tree instead.)
  2. Wait until the server answers (editor load takes a minute):
     ```bash
     until curl -s -m 2 -o /dev/null http://127.0.0.1:8000/mcp; do sleep 5; done; echo UP
     ```
  3. **Restart Claude Code** so the `mcp__unreal-editor__*` tools load. (Tools only
     register at session start — launching the editor mid-session is not enough to use
     them natively.)

> If the native `mcp__unreal-editor__*` tools still aren't available, you can drive the
> same endpoint over raw HTTP with `curl` (Streamable HTTP, JSON-RPC `initialize` →
> `notifications/initialized` → `tools/call`; pass the returned `Mcp-Session-Id` header;
> `tools/call` replies as SSE `data:` lines).

## 2. Discover the right tool

Never guess tool names or argument schemas — discover them:

1. `mcp__unreal-editor__list_toolsets` → pick the toolset (e.g.
   `editor_toolset.toolsets.scene.SceneTools`, `...blueprint.BlueprintTools`,
   `...material.MaterialTools`, `...object.ObjectTools`, `EditorToolset.EditorAppToolset`,
   `EditorToolset.LogsToolset`, `NiagaraToolsets.*`,
   `animation_toolset.toolsets.sequencer.SequencerTools`, `PCGToolset.*`,
   `UMGToolSet.UMGToolSet`, …).
2. `mcp__unreal-editor__describe_toolset { "toolset_name": "<toolset>" }` → exact
   `tool_name`s + input schemas.

## 3. Call a tool

```json
mcp__unreal-editor__call_tool {
  "toolset_name": "editor_toolset.toolsets.scene.SceneTools",
  "tool_name": "get_current_level",          // SHORT name, no toolset prefix
  "arguments": { }                            // must match the schema from describe_toolset
}
```

## Gotchas (don't skip)

- **Editor must stay running** — the server lives in-process; closing the editor kills the connection.
- **Property edits:** before `ObjectTools.set_properties` / UMG edits, ALWAYS call
  `ObjectTools.list_properties` first — names vary per class and can't be guessed; wrong names
  fail silently.
- **Confirm mutating calls.** Many tools change the project (`SceneTools.remove_from_scene`,
  `merge_actors`, `EditorAppToolset.StartPIE/StopPIE`, asset edits). Surface what you're about
  to do before running it unless the user said go ahead.
- **EULA:** data sent through this plugin to the LLM is UE Licensed Technology — user is
  responsible re: their LLM provider not training on it.
- Pointers returned by tools come back as `{"refPath": "..."}` — pass them straight back into
  other tools.
