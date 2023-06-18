"""Run once with UnrealEditor-Cmd -run=pythonscript -script=<this file>."""
import unreal

material_path = "/Game/Endless/Materials/M_ClimbSolid"
if not unreal.EditorAssetLibrary.does_asset_exist(material_path):
    unreal.EditorAssetLibrary.duplicate_asset(
        "/Game/LevelPrototyping/Materials/M_Solid", material_path)
material = unreal.load_asset(material_path)
if not material:
    raise RuntimeError("Could not prepare the instanced cliff material")
material.set_editor_property("used_with_instanced_static_meshes", True)
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_asset(material_path)

map_path = "/Game/Endless/Maps/EndlessClimb"
level_system = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not unreal.EditorAssetLibrary.does_asset_exist(map_path):
    if not level_system.new_level(map_path):
        raise RuntimeError("Could not create the endless climbing map")
else:
    level_system.load_level(map_path)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
mode = unreal.load_class(None, "/Script/Assignment.EndlessClimbGameMode")
if not mode:
    raise RuntimeError("Build AssignmentEditor before generating the map")
world.get_world_settings().set_editor_property("default_game_mode", mode)
if not unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerStart):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, -190, 20300))
level_system.save_current_level()
unreal.log("VYNIX_MAP_READY: " + map_path)
