import unreal, os
exec(open(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),'Tools/LandmassDemo/lmlib.py'),encoding='utf-8').read())
import sys
def S(n):
    a=find('언덕_1')
    if n=='hill': 
        clear_brushes(); a=make('언덕_1', circle(0,0,1500,4000)); a.set_editor_property('Cap Shape',False); register(a)
    elif n=='cap': a.set_editor_property('Cap Shape',True)
    elif n=='up': a.set_actor_location(a.get_actor_location()+unreal.Vector(0,0,1500),False,False)
    elif n=='down': a.set_actor_location(a.get_actor_location()-unreal.Vector(0,0,1500),False,False)
    elif n=='soft': a.set_editor_property('Falloff Angle',20.0)
    elif n=='steep': a.set_editor_property('Falloff Angle',70.0)
    elif n=='mid': a.set_editor_property('Falloff Angle',45.0)
    elif n=='shape':
        z=a.get_actor_location().z if False else spline(a).get_location_at_spline_point(0,unreal.SplineCoordinateSpace.WORLD).z
        set_shape(a,[(-4500,-2500,z),(2500,-3500,z),(5000,500,z),(1500,4000,z),(-3000,3000,z)])
    elif n=='terrace': a.set_editor_property('Terrace Alpha',1.0); a.set_editor_property('Terrace Spacing',400.0)
    elif n=='noterrace': a.set_editor_property('Terrace Alpha',0.0)
    elif n=='pond':
        b=make('연못_1', circle(-5000,4000,-600,2200)); b.set_editor_property('Cap Shape',True); register(b); a=b
    elif n=='hill2':
        b=make('작은언덕_2', circle(5500,-5000,800,1600)); b.set_editor_property('Cap Shape',True); register(b); a=b
    elif n=='flat': clear_brushes(); a=None
    elif n=='move2': a=find('작은언덕_2'); a.set_actor_location(a.get_actor_location()+unreal.Vector(-3500,-1500,0),False,False)
    elif n=='delpond': EAS().destroy_actor(find('연못_1')); a=find('언덕_1')
    if a: refresh(a); EAS().set_selected_level_actors([a]) if a else None
S(STEP)
