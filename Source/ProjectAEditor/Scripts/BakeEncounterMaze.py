"""Bake maze variants into editable Blueprints BP_EncounterMaze_V0~V7.

미로 변형을 편집 가능한 Blueprint BP_EncounterMaze_V0~V7로 굽습니다.
기존 Blueprint는 수동 수정을 보존하며 -EncounterMazeOverwrite 인자를 줄 때만 다시 만듭니다.
"""
import os
import unreal

overwrite = "-encountermazeoverwrite" in unreal.SystemLibrary.get_command_line().lower()
count, report = unreal.EncounterMazeBakeLibrary.bake_encounter_maze_blueprints(overwrite)
output = os.path.join(unreal.Paths.project_saved_dir(), "Automation", "EncounterMaze")
os.makedirs(output, exist_ok=True)
with open(os.path.join(output, "Bake.txt"), "w", encoding="utf-8") as handle:
    handle.write(f"baked={count} overwrite={overwrite}\n{report}")
unreal.log(f"[EncounterMaze] baked={count}\n{report}")
