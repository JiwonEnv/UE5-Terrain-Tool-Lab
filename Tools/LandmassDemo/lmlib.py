import unreal, math
EAS=lambda: unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
BR='/Landmass/Landscape/BlueprintBrushes/CustomBrush_Landmass.CustomBrush_Landmass_C'
def find(label):
    for a in EAS().get_all_level_actors():
        if a.get_actor_label()==label: return a
def spline(a): return a.get_component_by_class(unreal.SplineComponent)
def set_shape(a, pts, closed=True):
    sp=spline(a); sp.clear_spline_points(False)
    for p in pts: sp.add_spline_point(unreal.Vector(*p), unreal.SplineCoordinateSpace.WORLD, False)
    sp.set_closed_loop(closed, False); sp.update_spline()
def circle(cx,cy,z,r,n=8): return [(cx+r*math.cos(2*math.pi*i/n), cy+r*math.sin(2*math.pi*i/n), z) for i in range(n)]
def rect(cx,cy,z,w,h): return [(cx-w/2,cy-h/2,z),(cx+w/2,cy-h/2,z),(cx+w/2,cy+h/2,z),(cx-w/2,cy+h/2,z)]
def refresh(a):
    a.set_actor_location(a.get_actor_location(), False, False)
    for m in ("Construct Splines","Update Values","Brush Setup"):
        try: a.call_method(m)
        except Exception as e: print("call",m,e)
    try: a.request_landscape_update()
    except Exception as e: print('req', e)
def make(label, pts, closed=True, cls=BR):
    old=find(label)
    if old: EAS().destroy_actor(old)
    c=unreal.load_class(None, cls)
    a=EAS().spawn_actor_from_class(c, unreal.Vector(*pts[0]))
    a.set_actor_label(label); set_shape(a, pts, closed); refresh(a); return a
def show(*labels):
    EAS().set_selected_level_actors([find(l) for l in labels if find(l)])
def LS(): return [a for a in EAS().get_all_level_actors() if a.get_class().get_name()=='Landscape'][0]
def MGR(): return [a for a in EAS().get_all_level_actors() if 'BrushManager' in a.get_class().get_name()][0]
def register(a):
    try: a.set_editor_property('BrushManager', MGR())
    except Exception as e: print('mgr', e)
    ok=unreal.TerrainLabLibrary.add_brush_to_landscape(LS(), a, 0)
    refresh(a); return ok
def clear_brushes():
    for a in EAS().get_all_level_actors():
        n=a.get_class().get_name()
        if n.startswith('CustomBrush') or n.startswith('LandmassBrush_'): EAS().destroy_actor(a)
