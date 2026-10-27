import time, random
LAYOUT=dict(mountain=(8500,0,3000,900), t1=(0,0,0,2400,2400), t2=(2400,-1000,240,1600,1400), t3=(3900,1000,480,1400,1400), pond=(-2300,2300,-150,500))
def build():
    t=time.time(); clear_brushes(); L=LAYOUT
    x,y,z,r=L['mountain']; brush('산', circle(x,y,z,r,10), cap=True, angle=25)
    for k,n in (('t1','시작터_12x12칸'),('t2','둘째터'),('t3','셋째터')):
        x,y,z,w,h=L[k]; brush(n, rect(x,y,z,w,h), angle=60, lin=True)
    x,y,z,r=L['pond']; brush('연못', circle(x,y,z,r,8), angle=45)
    print('build_s', round(time.time()-t,2))
def edit():
    t=time.time()
    a=find('둘째터'); z=spline(a).get_location_at_spline_point(0,unreal.SplineCoordinateSpace.WORLD).z
    x,y,_,w,h=LAYOUT['t2']; set_shape(a, rect(x+200,y,z,w+400,h)); linear(a); refresh(a)
    m=find('산'); m.set_actor_location(m.get_actor_location()-unreal.Vector(0,0,500),False,False); refresh(m)
    print('edit_s', round(time.time()-t,2))
SH='/Engine/BasicShapes/'; MT='/Game/CozyRealm/Environment/Materials/'
def prop(label, shape, loc, scale, mat, folder='꾸밈'):
    a=EAS().spawn_actor_from_object(unreal.load_asset(SH+shape), unreal.Vector(*loc))
    a.set_actor_label(label); a.set_actor_scale3d(unreal.Vector(*scale)); a.set_folder_path(folder)
    a.static_mesh_component.set_material(0, unreal.load_asset(MT+mat)); return a
def inside(x,y):
    L=LAYOUT
    for k in ('t1','t2','t3'):
        cx,cy,_,w,h=L[k]
        if abs(x-cx)<w/2+300 and abs(y-cy)<h/2+300: return True
    cx,cy,_,r=L['pond']; return (x-cx)**2+(y-cy)**2<(r+300)**2
def undress():
    for a in EAS().get_all_level_actors():
        if str(a.get_folder_path()).startswith('꾸밈'): EAS().destroy_actor(a)
def dress(seed=7):
    undress(); random.seed(seed); n=0
    while n<70:
        x=random.uniform(-9000,12000); y=random.uniform(-9000,9000)
        if inside(x,y): continue
        z=ground_z(x,y)
        if z is None: continue
        s=random.uniform(0.8,1.4)
        prop('나무줄기', 'Cylinder', (x,y,z+100*s), (0.35*s,0.35*s,2.0*s), 'MI_Env_Trunk')
        prop('나무잎', 'Sphere', (x,y,z+300*s), (2.4*s,2.4*s,2.2*s), random.choice(['MI_Env_Leaf','MI_Env_LeafLight']))
        n+=1
    prop('시설_신사_3x3칸','Cube',(-500,-500,175),(6,6,3.5),'MI_Env_Stone')
    prop('시설_밭_2x2칸','Cube',(500,600,10),(4,4,0.2),'MI_Env_Dirt')
    prop('시설_제분소_2x2칸','Cube',(-600,700,150),(4,4,3),'MI_Env_Stone')
    prop('주인공_140cm','Cylinder',(300,-300,70),(0.5,0.5,1.4),'MI_Env_Hedge')
    x,y,z,r=LAYOUT['pond']; prop('연못물','Plane',(x,y,-80),(r*2/100*0.9,r*2/100*0.9,1),'MI_Env_Water')
    print('dressed')
