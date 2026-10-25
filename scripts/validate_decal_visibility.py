"""Focused decal inspection checks; does not run the regression suite.

Uses the E195 cockpit and every texture* directory next to its model directory.
Writes review captures, UI/picking reports, and results.json under build/review.
"""
import hashlib
import json
from pathlib import Path
import harness_process as subprocess
# Pillow only needs its native image routines here; avoid optional NumPy imports.
import sys
sys.modules['numpy'] = None
from PIL import Image

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / 'build/review'
EXE = REPO / 'build/bin/Release/Glider.exe'
BASE = REPO / 'build/baselines/072d453.exe'
MODEL = Path('F:/new-fss-aircraft-ejet/msfs24-projects/fss-aircraft-e195/SimObjects/Airplanes/E195/model/E195-Cockpit-LOD00.gltf')
NODE = 'DECAL_PEDESTAL'
COMMON = [str(MODEL), '--isolated', '--window-size', '1280x800', '--aa', 'none', '--vsync', 'off', '--fixed-frame-time', '0.05']
for directory in sorted(MODEL.parent.parent.glob('texture*')):
    if directory.is_dir():
        COMMON += ['--search-path', str(directory)]


def run(exe, name, extra=(), script=None):
    command = [str(exe), *COMMON, *extra]
    if script is None:
        command += ['--dump-frame', str(OUT / f'{name}.png'), '--capture-viewport', str(OUT / f'{name}_viewport.png'), '--capture-frame', '6', '--exit-after-dump']
    else:
        path = OUT / f'{name}.txt'
        path.write_text(script, encoding='utf-8')
        command += ['--ui-script', str(path), '--ui-report', str(OUT / f'{name}.json')]
    result = subprocess.run(command, cwd=REPO, capture_output=True, timeout=600)
    (OUT / f'{name}.log').write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(f'{name}: exit {result.returncode}; see log')
    if script is not None:
        report = json.loads((OUT / f'{name}.json').read_text())
        assert not report['failed'], report.get('errors')
        return report



def check_alpha():
    """Zero-alpha BLEND/MASK quads must have exactly the opaque wire silhouette."""
    import base64
    import struct
    global COMMON
    saved_common = COMMON
    data = struct.pack('<12f6H', -1,-1,0, 1,-1,0, 1,1,0, -1,1,0, 0,1,2, 0,2,3)
    fixture = {
        'asset': {'version': '2.0'}, 'scene': 0, 'scenes': [{'nodes': [0]}],
        'nodes': [{'name': 'Quad', 'mesh': 0}],
        'meshes': [{'primitives': [{'attributes': {'POSITION': 0}, 'indices': 1, 'material': 0}]}],
        'buffers': [{'uri': 'data:application/octet-stream;base64,' + base64.b64encode(data).decode(), 'byteLength': len(data)}],
        'bufferViews': [{'buffer': 0, 'byteLength': 48}, {'buffer': 0, 'byteOffset': 48, 'byteLength': 12}],
        'accessors': [{'bufferView': 0, 'componentType': 5126, 'count': 4, 'type': 'VEC3', 'min': [-1,-1,0], 'max': [1,1,0]},
                      {'bufferView': 1, 'componentType': 5123, 'count': 6, 'type': 'SCALAR'}],
    }
    captures = {}
    try:
        for mode in ['OPAQUE', 'BLEND', 'MASK']:
            fixture['materials'] = [{'doubleSided': True, 'alphaMode': mode, 'alphaCutoff': 0.5,
                                     'pbrMetallicRoughness': {'baseColorFactor': [1,1,1,0]}}]
            path = OUT / f'alpha_{mode}.gltf'
            path.write_text(json.dumps(fixture))
            COMMON = [str(path), '--isolated', '--window-size', '1280x800', '--aa', 'none', '--vsync', 'off']
            name = f'decalvis_alpha_{mode}'
            run(EXE, name, ['--render-mode', 'wireframe'])
            captures[mode] = (OUT / f'{name}_viewport.png').read_bytes()
            # Overlay on an invisible filled mesh should still show bright white edges.
            report = run(EXE, name + '_overlay', script=f'wait 12\nvpsetting 1 wireframe on\nwait 12\nsnap overlay\nshot build/review/{name}_overlay.png\n')
            assert report['snaps'][-1]['glErrors'] == 0
            image = Image.open(OUT / f'{name}_overlay.png').convert('RGB').crop((300,100,1000,740))
            captures[mode + '_white'] = sum(r > 250 and g > 250 and b > 250 for r,g,b in image.getdata())
        return {'alpha_wire_byte_identical': captures['OPAQUE'] == captures['BLEND'] == captures['MASK'],
                'alpha_overlay_white_pixels': {mode: captures[mode + '_white'] for mode in ['OPAQUE','BLEND','MASK']}}
    finally:
        COMMON = saved_common


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    assert MODEL.is_file() and BASE.is_file()
    results = {}
    for name, extra in [('decalvis_isolated', ['--isolate-node', NODE]),
                        ('decalvis_isolated_wireframe', ['--isolate-node', NODE, '--render-mode', 'wireframe']),
                        ('decalvis_wireframe', ['--select-node', NODE, '--render-mode', 'wireframe']),
                        ('decalvis_shaded', [] )]:
        print(f'Rendering {name}', flush=True)
        run(EXE, name, extra)
    run(BASE, 'decalvis_baseline')
    current = (OUT / 'decalvis_shaded_viewport.png').read_bytes()
    baseline = (OUT / 'decalvis_baseline_viewport.png').read_bytes()
    results['shaded_byte_identical'] = current == baseline
    results['shaded_window_byte_identical'] = (OUT / 'decalvis_shaded.png').read_bytes() == (OUT / 'decalvis_baseline.png').read_bytes()
    results['shaded_sha256'] = hashlib.sha256(current).hexdigest()
    # Hide the node in an identically framed isolated view: the difference excludes
    # the grid/background and avoids mistaking the selection box for geometry.
    run(EXE, 'decalvis_hidden', ['--isolate-node', NODE, '--hide-node', NODE])
    hidden = Image.open(OUT / 'decalvis_hidden_viewport.png').convert('RGB')
    for name in ['decalvis_isolated', 'decalvis_isolated_wireframe']:
        image = Image.open(OUT / f'{name}_viewport.png').convert('RGB')
        results[name + '_changed_pixels'] = sum(a != b for a, b in zip(image.getdata(), hidden.getdata()))
    overlay = run(EXE, 'decalvis_overlay_check', ['--select-node', NODE],
                  'wait 12\nvpsetting 1 wireframe on\nwait 12\nsnap overlay\nshot build/review/decalvis_wireframe_overlay.png\n')
    results['overlay_buffer_active'] = overlay['snaps'][-1]['decalBuffer']['active']
    # Real mouse clicks exercise the GPU picking pass, not the picknode test shortcut.
    script = 'wait 12\nclick 300 100\nwait 12\nsnap cleared\n'
    visible = Image.open(OUT / 'decalvis_isolated_viewport.png').convert('RGB')
    candidates = sorted(((sum(abs(a-b) for a,b in zip(visible.getpixel((x,y)), hidden.getpixel((x,y)))), x, y)
                         for y in range(visible.height) for x in range(visible.width)), reverse=True)
    points = []
    for difference, x, y in candidates:
        if difference < 30 or len(points) >= 12:
            break
        if all(abs(x-px) + abs(y-py) > 12 for px,py in points):
            points.append((x,y))
    # The viewport texture starts at window pixel (285, 70) in the fixed layout.
    for i, (x, y) in enumerate(( (x+285, y+70) for x,y in points )):
        script += f'click {x} {y}\nwait 12\nsnap pick{i}\n'
    picking = run(EXE, 'decalvis_picking', ['--isolate-node', NODE], script)
    results['picking_cleared_first'] = 'type=0|' in picking['snaps'][0]['selection']
    results['picking_selections'] = sorted({s['selection'] for s in picking['snaps'] if s['label'].startswith('pick')})
    results['isolated_buffer_inactive'] = all(not s['decalBuffer']['active'] for s in picking['snaps'])
    results['picking_gl_errors'] = max(s['glErrors'] for s in picking['snaps'])
    results['expected_node_id'] = next(i for i,n in enumerate(json.loads(MODEL.read_text())['nodes']) if n.get('name') == NODE)
    # Exercise exit/re-entry with the real Object shortcut and inspect buffer lifetime.
    lifecycle = run(EXE, 'decalvis_isolation_lifecycle', ['--isolate-node', NODE],
                    'wait 12\nsnap isolated\nkey slash\nwait 12\nsnap restored\nkey slash\nwait 12\nsnap reisolated\n')
    results['isolation_buffer_lifecycle'] = [snap['decalBuffer']['active'] for snap in lifecycle['snaps']]
    results.update(check_alpha())
    (OUT / 'decalvis_results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(json.dumps(results, indent=2), flush=True)
    assert results['isolation_buffer_lifecycle'] == [False, True, False]
    assert results['alpha_wire_byte_identical']
    assert min(results['alpha_overlay_white_pixels'].values()) > 100
    assert results['shaded_byte_identical']
    assert results['decalvis_isolated_changed_pixels'] > 100
    assert results['decalvis_isolated_wireframe_changed_pixels'] > 100
    assert results['overlay_buffer_active'] and results['isolated_buffer_inactive']
    assert results['picking_gl_errors'] == 0
    assert results['picking_cleared_first']
    assert any('node=' + NODE + '|' in s for s in results['picking_selections'])


if __name__ == '__main__':
    main()
