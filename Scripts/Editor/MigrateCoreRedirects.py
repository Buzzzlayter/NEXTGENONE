"""Resave a bounded native-module migration, then fresh-load without its redirects.

Normal Python: plan by default; --apply performs the migration with Editor-Cmd closed to GUI.
Unreal Python: internal resave/verify phase, selected via task-specific environment variables.
No PIE, Blueprint compilation, cook, gameplay test or plugin asset writes.
"""

import argparse
import datetime
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def write_json(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def asset_snapshot(asset):
    result = {"class": asset.get_class().get_path_name()}
    if isinstance(asset, __import__("unreal").Blueprint):
        parent = __import__("unreal").BlueprintEditorLibrary.get_blueprint_parent_class(asset)
        result["parent"] = parent.get_path_name() if parent else None
    if result["class"].endswith(".NGAbilitySet"):
        result["abilities"] = [
            {
                "class": grant.get_editor_property("ability").get_path_name() if grant.get_editor_property("ability") else None,
                "level": grant.get_editor_property("level"),
                "input_tag": str(grant.get_editor_property("input_tag").get_editor_property("tag_name")),
            }
            for grant in asset.get_editor_property("abilities")
        ]
        result["effects"] = [effect.get_path_name() if effect else None for effect in asset.get_editor_property("effects")]
    if result["class"].endswith(".NGWeaponDefinition"):
        result["appearance"] = {
            name: value.get_path_name() if value else None
            for name in ("mesh", "skeletal_mesh")
            for value in (asset.get_editor_property(name),)
        }
        result["appearance"]["attach_socket"] = str(asset.get_editor_property("attach_socket"))
        result["appearance"]["visual_transform"] = asset.get_editor_property("visual_transform").export_text()
    if isinstance(asset, __import__("unreal").Blueprint):
        unreal = __import__("unreal")
        default = unreal.get_default_object(asset.generated_class())
        if isinstance(default, unreal.NGInventoryItemDefinition):
            result["item"] = {
                "display_name": str(default.get_editor_property("display_name")),
                "max_stack_size": default.get_editor_property("max_stack_size"),
                "item_tags": default.get_editor_property("item_tags").export_text(),
                "fragments": [],
            }
            for fragment in default.get_editor_property("fragments"):
                if not fragment:
                    result["item"]["fragments"].append(None)
                    continue
                entry = {"class": fragment.get_class().get_path_name()}
                # Some non-BlueprintType fragment classes use a base Python wrapper;
                # their native property names remain accessible without snake-case mapping.
                field = {"NGInventoryFragment_Equipment": "Definition", "NGInventoryFragment_ItemActions": "ActivationAbility"}.get(entry["class"].split(".")[-1])
                if field:
                    value = fragment.get_editor_property(field)
                    entry[field] = value.get_path_name() if value else None
                result["item"]["fragments"].append(entry)
    return result


def editor_phase():
    import unreal

    manifest_path = Path(os.environ["NG_CORE_REDIRECT_MANIFEST"])
    phase = os.environ["NG_CORE_REDIRECT_PHASE"]
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    report = {"phase": phase, "success": False, "assets": {}, "errors": []}
    try:
        for redirect in manifest["redirects"]:
            kind = redirect.get("kind")
            if kind in ("GameName", "Package"):
                destination = unreal.load_package(redirect["new"])
            elif kind == "Property":
                owner, name = redirect["new"].rsplit(".", 1)
                destination = unreal.load_class(None, owner)
                if destination:
                    unreal.get_default_object(destination).get_editor_property(name)
            else:
                destination = unreal.load_object(None, redirect["new"])
            if not destination:
                raise RuntimeError("Missing destination type: " + redirect["new"])
        assets = unreal.get_editor_subsystem(unreal.EditorAssetSubsystem)
        for entry in manifest["assets"]:
            asset = assets.load_asset(entry["package"])
            if not asset:
                raise RuntimeError("Cannot load " + entry["package"])
            report["assets"][entry["package"]] = asset_snapshot(asset)
            if phase == "resave" and not assets.save_loaded_asset(asset, only_if_is_dirty=False):
                raise RuntimeError("Cannot save " + entry["package"])
            unreal.log("NG_REDIRECT_ASSET " + phase + " " + entry["package"])
        if phase == "verify":
            baseline = json.loads(manifest_path.with_name("resave.json").read_text(encoding="utf-8"))
            if report["assets"] != baseline["assets"]:
                raise RuntimeError("Asset classes, Blueprint parents, grants, item fragments or weapon appearance differ after redirect removal")
        report["success"] = True
    except Exception as error:
        report["errors"].append(str(error))
        unreal.log_error("NG_REDIRECT_FAILED " + str(error))
        raise
    finally:
        write_json(manifest_path.with_name(phase + ".json"), report)
    unreal.log("NG_REDIRECT_DONE " + phase + " count=" + str(len(report["assets"])))


def process_running():
    output = subprocess.check_output([
        "powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
        "Get-Process | Where-Object { $_.ProcessName -eq 'UnrealEditor' } | Select-Object -ExpandProperty Id",
    ])
    return bool(output.strip())


def contains_name(data, value):
    return value.encode("utf-8") in data or value.encode("utf-16-le") in data


def plan(project, old_module, new_module):
    root = project.parent
    config = root / "Config/DefaultEngine.ini"
    text = config.read_text(encoding="utf-8-sig")
    redirects = []
    for line in text.splitlines():
        match = re.fullmatch(r'\s*\+(Class|Struct)Redirects=\(OldName="([^"]+)",NewName="([^"]+)"\)\s*', line)
        if match and match[2].startswith("/Script/" + old_module + ".") and match[3].startswith("/Script/" + new_module + "."):
            redirects.append({"line": line, "old": match[2], "new": match[3]})
    if not redirects:
        raise RuntimeError("No exact Class/Struct redirects for this module pair; nothing changed")
    names = [entry["old"].split(".")[-1] for entry in redirects]
    content_roots = [(root / "Content", True)]
    content_roots.extend((plugin.parent / "Content", False) for plugin in (root / "Plugins").rglob("*.uplugin"))
    candidates = []
    plugin_refs = []
    for content, writable in content_roots:
        if not content.is_dir():
            continue
        for file in content.rglob("*"):
            if file.suffix.lower() not in (".uasset", ".umap"):
                continue
            data = file.read_bytes()
            direct = contains_name(data, "/Script/" + old_module) and any(contains_name(data, name) for name in names)
            relative = file.relative_to(content)
            # Include project-owned Blueprint dependents in the existing gameplay folders.
            blueprint = writable and relative.parts[0] in ("Blueprints", "Gameplay") and any(
                contains_name(data, token + "\0") for token in ("Blueprint", "WidgetBlueprint", "AnimBlueprint")
            )
            if direct and not writable:
                plugin_refs.append(str(file.relative_to(root)))
            if writable and (direct or blueprint):
                candidates.append({"file": str(file.relative_to(root)), "package": "/Game/" + relative.with_suffix("").as_posix()})
    if plugin_refs:
        raise RuntimeError("Potential plugin references require separate review; plugins will not be written: " + ", ".join(plugin_refs))
    return {"project": str(project), "config": str(config), "redirects": redirects, "assets": sorted(candidates, key=lambda item: item["package"])}


def run_editor(editor, project, manifest, phase):
    log = manifest.with_name(phase + ".log")
    env = os.environ.copy()
    env["NG_CORE_REDIRECT_MANIFEST"] = str(manifest)
    env["NG_CORE_REDIRECT_PHASE"] = phase
    command = [str(editor), str(project), "-run=pythonscript", "-script=" + str(Path(__file__).resolve()),
               "-unattended", "-nop4", "-SCCProvider=None", "-NullRHI", "-nosound", "-NoLiveCoding", "-NoZenAutoLaunch",
               "-ddc=InstalledNoZenLocalFallback", "-LocalDataCachePath=" + str(project.parent / "Saved/DerivedDataCache"),
               "-abslog=" + str(log)]
    with manifest.with_name(phase + "-console.log").open("w", encoding="utf-8") as output:
        completed = subprocess.run(command, env=env, stdout=output, stderr=subprocess.STDOUT)
    report_path = manifest.with_name(phase + ".json")
    report = json.loads(report_path.read_text(encoding="utf-8")) if report_path.exists() else {}
    if completed.returncode or not report.get("success"):
        raise RuntimeError(phase + " failed; see " + str(log) + "; " + str(report.get("errors", [])))
    # A non-null Blueprint alone does not prove all imports resolved. Reject scoped linker errors too.
    target_names = [redirect["old"].split(".")[1] if redirect.get("kind") == "Property" else redirect["old"].split(".")[-1]
                    for redirect in json.loads(manifest.read_text(encoding="utf-8"))["redirects"]]
    for line in log.read_text(encoding="utf-8", errors="replace").splitlines():
        if re.search(r"(?i)(error:|failed to load|unknown structure|could not find|failed to find|verifyimport)", line) and any(name in line for name in target_names):
            raise RuntimeError("Unresolved migration reference: " + line)
    print(phase + ": PASS, assets=" + str(len(report["assets"])), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=Path, default=Path.cwd() / "NEXTGENONE.uproject")
    parser.add_argument("--old-module")
    parser.add_argument("--new-module")
    parser.add_argument("--plan-file", type=Path, help="Explicit reviewed plan for mixed Class/Struct/Property/Package/GameName redirects")
    parser.add_argument("--engine-root", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    project = args.project.resolve(strict=True)
    if args.plan_file:
        manifest_data = json.loads(args.plan_file.read_text(encoding="utf-8"))
        if Path(manifest_data["project"]).resolve() != project or manifest_data.get("plugin_candidates"):
            raise RuntimeError("Plan project mismatch or unresolved plugin candidates")
        config = project.parent / "Config/DefaultEngine.ini"
        if Path(manifest_data["config"]).resolve() != config:
            raise RuntimeError("Plan config mismatch")
        lines = set(config.read_text(encoding="utf-8-sig").splitlines())
        if not manifest_data["redirects"] or any(r["line"] not in lines for r in manifest_data["redirects"]):
            raise RuntimeError("Redirect plan is stale")
        for entry in manifest_data["assets"]:
            file = (project.parent / entry["file"]).resolve(strict=True)
            if not file.is_relative_to(project.parent / "Content") or file.suffix not in (".uasset", ".umap"):
                raise RuntimeError("Plan asset outside project Content: " + str(file))
            expected = "/Game/" + file.relative_to(project.parent / "Content").with_suffix("").as_posix()
            if entry["package"] != expected:
                raise RuntimeError("Plan package mismatch: " + entry["package"])
    else:
        if not args.old_module or not args.new_module:
            parser.error("Specify --old-module and --new-module, or --plan-file")
        manifest_data = plan(project, args.old_module, args.new_module)
    print("Plan: redirects=" + str(len(manifest_data["redirects"])) + ", assets=" + str(len(manifest_data["assets"])), flush=True)
    if not args.apply:
        for entry in manifest_data["assets"]:
            print(entry["package"])
        return
    if process_running():
        raise RuntimeError("Close Unreal Editor after saving user changes; no files changed")
    if not args.engine_root:
        raise RuntimeError("--engine-root is required with --apply")
    editor = args.engine_root / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
    if not editor.is_file():
        raise RuntimeError("Missing " + str(editor))
    backup = project.parent / "Saved/CoreRedirectMigrations" / datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
    backup.mkdir(parents=True, exist_ok=False)
    manifest = backup / "manifest.json"
    write_json(manifest, manifest_data)
    config = Path(manifest_data["config"])
    original_config = config.read_bytes()
    (backup / "DefaultEngine.ini").write_bytes(original_config)
    for entry in manifest_data["assets"]:
        source = project.parent / entry["file"]
        destination = backup / entry["file"]
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
    print("Backup: " + str(backup), flush=True)
    run_editor(editor, project, manifest, "resave")
    if config.read_bytes() != original_config:
        raise RuntimeError("Config changed during resave; stopping without removing redirects")
    selected = {entry["line"] for entry in manifest_data["redirects"]}
    text = original_config.decode("utf-8-sig")
    migrated = "".join(line for line in text.splitlines(keepends=True) if line.rstrip("\r\n") not in selected)
    config.write_bytes(migrated.encode("utf-8"))
    try:
        run_editor(editor, project, manifest, "verify")
    except BaseException:
        config.write_bytes(original_config)
        print("Redirects restored; resaved assets and backups retained", flush=True)
        raise
    print("DONE: removed " + str(len(selected)) + " selected redirects; " + str(backup), flush=True)


if os.environ.get("NG_CORE_REDIRECT_PHASE"):
    editor_phase()
elif __name__ == "__main__":
    main()
