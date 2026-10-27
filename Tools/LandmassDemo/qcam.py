import unreal, math
def qcam(fx=0,fy=0,fz=0,dist=8500,pitch=-45,yaw=45,fov=30, pilot=True):
    eas=unreal.get_editor_subsystem(unreal.EditorActorSubsystem); les=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    les.eject_pilot_level_actor()
    c=([a for a in eas.get_all_level_actors() if a.get_actor_label()=='게임카메라_쿼터뷰'] or [None])[0]
    if not c:
        c=eas.spawn_actor_from_class(unreal.CameraActor, unreal.Vector(0,0,0)); c.set_actor_label('게임카메라_쿼터뷰')
    p,y=math.radians(pitch),math.radians(yaw)
    f=unreal.Vector(math.cos(p)*math.cos(y), math.cos(p)*math.sin(y), math.sin(p))
    c.set_actor_location_and_rotation(unreal.Vector(fx,fy,fz)-f*dist, unreal.Rotator(roll=0,pitch=pitch,yaw=yaw), False, False)
    c.camera_component.set_editor_property('field_of_view', fov); c.camera_component.set_editor_property('constrain_aspect_ratio', False)
    if pilot: les.pilot_level_actor(c)
    return c
