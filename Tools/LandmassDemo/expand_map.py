import unreal
# 영지 확장 비교 맵: L_TileB1의 조명을 복사 · 칸 땅 6개(① 계단식 터 3단계 · ② 정사각형 3단계)를 나란히
def _make():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in eas.get_all_level_actors():
        n = a.get_class().get_name()
        if n in ('CozyTileTerrain', 'StaticMeshActor', 'CameraActor'): eas.destroy_actor(a)
    cls = unreal.load_class(None, '/Script/TerrainLab.CozyTileTerrain')
    for row, kind in ((0, 1), (1, 2)):
        for stage in (1, 2, 3):
            t = eas.spawn_actor_from_class(cls, unreal.Vector((stage - 1) * 12000 - 4000, row * 14000 - 4000, 0))
            t.set_actor_label(f'확장{kind}_{stage}단계')
            t.set_editor_property('grid_size', unreal.IntPoint(40, 40))
            t.set_editor_property('open_min', unreal.IntPoint(0, 0)); t.set_editor_property('open_max', unreal.IntPoint(39, 39))
            s = t.get_editor_property('settings'); s.set_editor_property('level_heights', [0.0, 240.0, 480.0, 720.0, 960.0]); t.set_editor_property('settings', s)
            t.set_editor_property('save_name', f'Expand{kind}_{stage}')
    print('saved', les.save_current_level())
_make()
