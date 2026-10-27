"""시험 에디터(TerrainLab)에 파이썬 파일이나 한 줄 코드를 원격으로 실행한다.
사용: python uepy.py file 경로.py  |  python uepy.py code "print(1)"
"""
import sys, time
sys.path.append(r"C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\PythonScriptPlugin\Content\Python")
import remote_execution as rx

mode, arg = sys.argv[1], sys.argv[2]
r = rx.RemoteExecution(); r.start()
try:
    t = time.time()
    while not r.remote_nodes and time.time() - t < 20:
        time.sleep(0.5)
    nodes = [n for n in r.remote_nodes if 'TerrainLab' in n.get('project_name', '')] or r.remote_nodes
    if not nodes:
        print('NO_NODE'); sys.exit(2)
    r.open_command_connection(nodes[0]['node_id'])
    if mode == 'file':
        cmd = open(arg, encoding='utf-8').read(); em = rx.MODE_EXEC_FILE
    else:
        cmd = arg; em = rx.MODE_EXEC_FILE
    res = r.run_command(cmd, unattended=True, exec_mode=em)
    for o in res.get('output', []):
        print(o.get('output', '').rstrip())
    print('SUCCESS' if res.get('success') else 'FAIL', res.get('result', ''))
finally:
    r.stop()
