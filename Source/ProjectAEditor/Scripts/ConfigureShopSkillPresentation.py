import json
import sys
from pathlib import Path

import unreal

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from ConfigureWarriorContent import configure_weapon, load, require, save
from WarriorContentPaths import ROOT, SWORD_FOLDER, SWORD_SOURCE, SWORD_RECOVERY_SOURCE, SWORD_SOURCE_MESH, WARRIOR_MONTAGE, WEAPON_SOURCE, mirrored_path


def configure():
    player = load(ROOT + "/Blueprint/Unit/BP_PlayerUnit")
    defaults = unreal.get_default_object(player.generated_class())
    mesh_component = defaults.get_editor_property("mesh")
    previous_mesh = mesh_component.get_editor_property("skeletal_mesh_asset")
    mesh = load(SWORD_SOURCE_MESH)
    require(previous_mesh.get_name() == mesh.get_name(), "Shared player must retain the same Manny mesh variant")
    skeleton = mesh.get_editor_property("skeleton")
    previous_skeleton = previous_mesh.get_editor_property("skeleton")
    previous_skills = list(defaults.get_editor_property("equipped_skill_data_assets"))
    # Source references must already be migrated; do not add compatibility to the original skeleton.
    # 원본 참조 이전이 완료되어 있어야 하며 원본 스켈레톤에 호환 설정을 추가하지 않습니다.
    require(skeleton == previous_skeleton, "Shared player must already use the original Manny skeleton")
    mesh_component.set_editor_property("skeletal_mesh_asset", mesh)
    attack = load(SWORD_SOURCE)
    recovery = load(SWORD_RECOVERY_SOURCE)
    montage_path = SWORD_FOLDER + "/AM_SwordAttack_Manny"
    if unreal.EditorAssetLibrary.does_asset_exist(montage_path):
        montage = load(montage_path)
    else:
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property("target_skeleton", skeleton)
        factory.set_editor_property("source_animation", attack)
        montage = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset("AM_SwordAttack_Manny", SWORD_FOLDER, unreal.AnimMontage, factory), "Could not create Manny sword montage")
    require(unreal.WarriorAssetLibrary.configure_sword_montage(montage, attack, recovery, 0.2), "Could not author Manny sword montage")
    save(montage)
    overrides = dict(defaults.get_editor_property("round_montage_overrides"))
    overrides[load(WARRIOR_MONTAGE)] = montage
    defaults.set_editor_property("round_montage_overrides", overrides)
    unreal.BlueprintEditorLibrary.compile_blueprint(player)
    configure_weapon(player, load(mirrored_path(WEAPON_SOURCE)), (-11.095651, 5.605028, -10.0))
    unreal.BlueprintEditorLibrary.compile_blueprint(player)
    save(player)
    defaults = unreal.get_default_object(player.generated_class())
    require(list(defaults.get_editor_property("equipped_skill_data_assets")) == previous_skills, "Historical player defaults must remain unchanged")
    report = {"player": player.get_path_name(), "mesh": mesh.get_path_name(), "previous_mesh": previous_mesh.get_path_name(), "skeleton": skeleton.get_path_name(), "previous_skeleton": previous_skeleton.get_path_name(), "montage": montage.get_path_name(), "seconds": montage.get_editor_property("sequence_length"), "legacy_skill_count": len(previous_skills), "gameplay_test": "not run"}
    output = Path(unreal.Paths.project_saved_dir(), "Automation", "ShopSkillPresentationConfigure.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log("SHOP_SKILL_PRESENTATION_CONFIGURED " + str(output))


if __name__ == "__main__":
    configure()
