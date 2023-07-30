"""Retarget four arcade clips to Payton. Requires a full editor (-ExecutePythonScript).

UE 5.5's batch retargeter opens the Content Browser, so -run=pythonscript is unsupported.
"""
import unreal

assets = unreal.EditorAssetLibrary
folder = '/Game/Endless/Payton'
source = unreal.load_asset('/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple')
target = unreal.load_asset('/Game/MetaHumans/Payton/Female/Medium/NormalWeight/Body/f_med_nrw_body')
retarget_path = folder + '/RTG_ClimbPayton'
retarget = unreal.load_asset(retarget_path) if assets.does_asset_exist(retarget_path) else assets.duplicate_asset(
    '/Game/MetaHumans/Common/Common/RTG_metahuman', retarget_path)
controller = unreal.IKRetargeterController.get_controller(retarget)
controller.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, unreal.load_asset('/Game/Characters/Mannequins/Rigs/IK_Mannequin'))
controller.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, unreal.load_asset('/Game/MetaHumans/Common/Common/IK_metahuman'))
controller.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, source)
controller.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, target)
controller.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
assets.save_loaded_asset(retarget)

for name in ['Hang_Idle', 'JumpLeft', 'JumpRight', 'Jump_Up']:
    destination = folder + '/A_Payton_' + name
    if assets.does_asset_exist(destination):
        continue
    clip = assets.find_asset_data('/Game/new_animation/exported/' + name)
    result = unreal.IKRetargetBatchOperation.duplicate_and_retarget(
        [clip], source, target, retarget, prefix='Payton_', include_referenced_assets=False)
    if len(result) != 1:
        raise RuntimeError('Expected one retargeted animation for ' + name)
    animation = result[0].get_asset()
    animation.set_editor_property('force_root_lock', True)
    assets.save_loaded_asset(animation)
    if not assets.rename_asset(animation.get_path_name(), destination):
        raise RuntimeError('Could not move the retargeted animation: ' + name)
    assets.save_asset(destination)
    unreal.log('VYNIX_PAYTON: saved ' + destination)
