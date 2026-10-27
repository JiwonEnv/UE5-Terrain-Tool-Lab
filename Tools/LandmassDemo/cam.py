import unreal, math, sys
def look(px,py,pz,tx,ty,tz):
    dx,dy,dz=tx-px,ty-py,tz-pz
    yaw=math.degrees(math.atan2(dy,dx)); pitch=math.degrees(math.atan2(dz,math.hypot(dx,dy)))
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(unreal.Vector(px,py,pz), unreal.Rotator(roll=0,pitch=pitch,yaw=yaw))
look(*CAM)
