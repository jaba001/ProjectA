import hashlib
import json
from pathlib import Path

import unreal


ROOT = Path(unreal.Paths.project_dir()).resolve()
SPEC = json.loads(Path(__file__).with_name("DrGameSkillSpecs.json").read_text(encoding="utf-8"))
AUTHOR = "-SkillVfxDirectionAuthor" in unreal.SystemLibrary.get_command_line()
ASSETS = unreal.EditorAssetLibrary
OWNER_KEY = "ProjectA.SkillVfxDirection"
OWNER = "ModuleSpace.v1"
REPORT_DIR = ROOT / "Saved/Automation/SkillVfxDirection"


def require(value, reason):
    if not value:
        raise RuntimeError(reason)
    return value


def file_hash(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(path):
    return require(unreal.load_asset(path), "Missing asset: " + path)


def inspection(asset):
    return json.loads(unreal.CombatVfxAssetLibrary.inspect_niagara_module_input_spaces(asset))


def input_record(report, entry):
    matches = [item for emitter in report["emitters"] if emitter["name"] == entry["emitter"] for module in emitter["modules"] if module["name"] == entry["module"] for item in module["inputs"] if item["name"].replace(" ", "") == entry["input"].replace(" ", "")]
    return require(matches[0] if len(matches) == 1 else None, "Module input must be unique: " + str(entry))


def visual_contract(asset):
    report = json.loads(unreal.CombatVfxAssetLibrary.inspect_niagara_space(asset))
    require(report["valid"] and report["systemValid"], "Niagara compiled data is invalid")
    emitters = [{key: emitter[key] for key in ["name", "enabled", "stateful", "localSpace", "simTarget", "renderers"]} for emitter in report["emitters"]]
    # Compiled caches can contain repeated audio interfaces; preserve each distinct sound and playback setting.
    # 컴파일 캐시에는 오디오 인터페이스 사본이 반복될 수 있으므로 서로 다른 사운드와 재생 설정을 모두 보존합니다.
    audio = sorted({json.dumps({key: value for key, value in item.items() if key != "object"}, sort_keys=True) for item in report["audioInterfaces"]})
    parameters = [{"emitter": emitter["name"], "parameters": script["parameters"]} for emitter in report["emitters"] for script in emitter["scripts"]]
    return {"emitters": emitters, "audio": audio, "user_parameters": report["userParameters"], "script_parameters": parameters}


def module_contract(report, entry, expected_local=False):
    emitters = []
    for emitter in report["emitters"]:
        modules = []
        for module in emitter["modules"]:
            inputs = [{key: item[key] for key in ["name", "alias", "type", "static", "hidden", "hasOverride", "valueKnown", "value", "space", "defaultVariable", "defaultMode"] if key in item} for item in module["inputs"]]
            if expected_local and emitter["name"] == entry["emitter"] and module["name"] == entry["module"]:
                for item in inputs:
                    if item["name"].replace(" ", "") == entry["input"].replace(" ", ""):
                        item.update({"hasOverride": True, "value": 2, "space": "Local"})
            modules.append({"name": module["name"], "script": module["script"], "version": module["versionGuid"], "inputs": sorted(inputs, key=lambda item: item["name"])})
        emitters.append({"name": emitter["name"], "enabled": emitter["enabled"], "modules": sorted(modules, key=lambda item: item["name"])})
    return sorted(emitters, key=lambda item: item["name"])


def main():
    # Author only the declared derivative and one local module override; source assets remain read-only.
    # 명시한 파생본과 모듈의 로컬 재정의 하나만 작성하며 원본 에셋은 읽기 전용으로 보존합니다.
    baseline = json.loads((ROOT / "Saved/Automation/DrGameSkills/OriginalHashes.json").read_text(encoding="utf-8"))
    require(len(baseline) == 1900 and all(file_hash(ROOT / filename) == expected for filename, expected in baseline.items()), "Purchased source baseline changed")
    skill_files = list((ROOT / "Content/User_JeHoon/Blueprint/DataAsset/Skills").rglob("*.uasset"))
    skill_hashes = {str(path.relative_to(ROOT)): file_hash(path) for path in skill_files}
    rows, created = [], []
    entries = SPEC.get("direction_derivatives", [])
    require(len(entries) == 1 and entries[0]["space"] == "Local", "Expected one reviewed local direction derivative")
    for entry in entries:
        require(entry["destination"].startswith("/Game/User_JeHoon/"), "Derivative must be project-owned")
        source = load(entry["source"])
        require(isinstance(source, unreal.NiagaraSystem), "Source must be Niagara")
        before = inspection(source)
        REPORT_DIR.mkdir(parents=True, exist_ok=True)
        (REPORT_DIR / "OriginalModuleSpaces.json").write_text(json.dumps(before, ensure_ascii=False, indent=2), encoding="utf-8")
        original_input = input_record(before, entry)
        source_contract = visual_contract(source)
        source_emitter = require(next((item for item in source_contract["emitters"] if item["name"] == entry["emitter"]), None), "Missing source emitter")
        require(original_input["valueKnown"] and (original_input["value"] == 1 or (original_input["value"] == 0 and not source_emitter["localSpace"])), "Original module input must resolve to world space")
        exists = ASSETS.does_asset_exist(entry["destination"])
        if exists:
            derivative = load(entry["destination"])
            require(isinstance(derivative, unreal.NiagaraSystem) and ASSETS.get_metadata_tag(derivative, OWNER_KEY) == OWNER and ASSETS.get_metadata_tag(derivative, "DirectionSource") == entry["source"], "Unowned derivative destination")
            existing_input = input_record(inspection(derivative), entry)
            require(existing_input["valueKnown"] and existing_input["value"] == 2, "Existing derivative has been modified; preserve it for review")
        else:
            require(AUTHOR, "Missing derivative; run the explicit authoring option")
            folder, name = entry["destination"].rsplit("/", 1)
            derivative = require(unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(name, folder, source), "Derivative duplication failed")
            ASSETS.set_metadata_tag(derivative, OWNER_KEY, OWNER)
            ASSETS.set_metadata_tag(derivative, "DirectionSource", entry["source"])
            created.append(entry["destination"])
        derivative_contract = visual_contract(derivative)
        (REPORT_DIR / "VisualContracts.json").write_text(json.dumps({"source": source_contract, "derivative": derivative_contract}, ensure_ascii=False, indent=2), encoding="utf-8")
        require(derivative_contract == source_contract, "Derivative changed unrelated visual data")
        if AUTHOR and not exists:
            error = unreal.CombatVfxAssetLibrary.configure_niagara_module_input_space(derivative, entry["emitter"], entry["module"], original_input["name"], unreal.NiagaraCoordinateSpace.LOCAL)
            require(not error, error)
        after = inspection(derivative)
        final_input = input_record(after, entry)
        require(final_input["valueKnown"] and final_input["value"] == 2, "Saved derivative input must be Local")
        require(module_contract(after, entry) == module_contract(before, entry, expected_local=True), "Direction authoring changed another coordinate input or module")
        require(visual_contract(derivative) == source_contract, "Direction authoring changed unrelated visual data")
        result, errors, warnings = unreal.get_editor_subsystem(unreal.EditorValidatorSubsystem).is_object_valid(derivative, unreal.DataValidationUsecase.SCRIPT)
        require(result != unreal.DataValidationResult.INVALID and not errors and not warnings, "Derivative data validation failed")
        if AUTHOR and not exists:
            require(ASSETS.save_loaded_asset(derivative, only_if_is_dirty=False), "Derivative save failed")
        package_file = ROOT / "Content" / (entry["destination"].removeprefix("/Game/") + ".uasset")
        rows.append({"source": entry["source"], "destination": entry["destination"], "source_input": original_input, "derivative_input": final_input, "bytes": package_file.stat().st_size, "sha256": file_hash(package_file), "data_validation": str(result), "source_inspection": before, "derivative_inspection": after})
    require(all(file_hash(ROOT / filename) == expected for filename, expected in baseline.items()), "Purchased source changed during authoring")
    require(all(file_hash(ROOT / filename) == expected for filename, expected in skill_hashes.items()), "Skill package changed during derivative authoring")
    report = {"mode": "author" if AUTHOR else "read_only", "created_packages": created, "derivatives": rows, "source_files_unchanged": len(baseline), "skill_files_unchanged": len(skill_hashes), "niagara_compile_and_field_checks": "passed", "gameplay_test": "not run"}
    REPORT_DIR.mkdir(parents=True, exist_ok=True)
    (REPORT_DIR / ("DerivativeAuthor.json" if AUTHOR else "DerivativeReload.json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("VFX_DIRECTION_DERIVATIVE " + json.dumps({"mode": report["mode"], "created": len(created), "derivatives": len(rows), "originals_unchanged": len(baseline), "skills_unchanged": len(skill_hashes)}))


if __name__ == "__main__":
    main()
