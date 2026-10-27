def plan(): return ([a for a in EAS().get_all_level_actors() if a.get_class().get_name()=='CozyTerrainLayout'] or [None])[0]
def lf(name, kind, c, s, h, blend=4.0, peak=0.0):
    f=unreal.CozyLandform()
    f.set_editor_properties({'name':name,'type':kind,'center':unreal.Vector2D(*c),'size':unreal.Vector2D(*s),'height':h,'blend':blend,'peak_flat':peak})
    return f
def setup():
    p=plan(); r=p.get_editor_property('recipe'); M=unreal.CozyLandformKind.MOUNTAIN; P=unreal.CozyLandformKind.PLATEAU
    r.set_editor_property('map_size', unreal.Vector2D(252,252))
    r.set_editor_property('landforms', [
        lf('산', M, (85,0), (150,150), 30.0, 8.0, 12.0),
        lf('시작터_12x12칸', P, (0,0), (24,24), 0.0, 2.0),
        lf('둘째터', P, (24,-10), (16,14), 2.4, 2.0),
        lf('셋째터', P, (39,10), (14,14), 4.8, 2.0),
        lf('연못', P, (-23,23), (10,10), -1.5, 3.0)])
    p.set_editor_property('recipe', r)
    p.set_editor_property('ground_material', unreal.load_asset('/Game/CozyRealm/Environment/Materials/MI_Env_Grass'))
    d=p.get_editor_property('recipe').to_dict(); print(list(d.keys())); print(str(d)[:600])
