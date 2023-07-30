"""Make an arcade version of the project's original BP_MyCharacter (Payton).

Keep its cosmetic construction and components. The native arcade parent supplies
gameplay, so the copied prototype EventGraph is removed. The original stays intact.
Run after compiling AssignmentEditor and preparing the Payton animation clips.
"""
import unreal

assets = unreal.EditorAssetLibrary
path = '/Game/Endless/Payton/BP_EndlessPayton'
if assets.does_asset_exist(path):
    blueprint = unreal.load_asset(path)
else:
    blueprint = assets.duplicate_asset('/Game/BP_MyCharacter', path)
    event_graph = unreal.BlueprintEditorLibrary.find_event_graph(blueprint)
    if event_graph:
        unreal.BlueprintEditorLibrary.remove_graph(blueprint, event_graph)
    parent = unreal.load_class(None, '/Script/Assignment.EndlessClimber')
    unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent)

unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
defaults = unreal.get_default_object(blueprint.generated_class())
for property_name, clip_name in [('hang_animation', 'Hang_Idle'), ('left_animation', 'JumpLeft'),
                                 ('right_animation', 'JumpRight'), ('up_animation', 'Jump_Up')]:
    clip = unreal.load_asset('/Game/Endless/Payton/A_Payton_' + clip_name)
    if not clip:
        raise RuntimeError('Missing Payton clip: ' + clip_name)
    defaults.set_editor_property(property_name, clip)
assets.save_loaded_asset(blueprint, only_if_is_dirty=False)
unreal.log('VYNIX_PAYTON: saved ' + path)
