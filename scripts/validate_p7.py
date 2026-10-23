# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Phase 7 IBL and render-quality validation.

Generates two fixtures in build/p7/: a sphere grid (metal, dielectric and sheen rows with
roughness 0..1) and a synthetic HDR whose 1-degree sun (2e5) exceeds half-float range.

Sections:
  quality   Captures the viewport for each quality variant (one mechanism per variant, plus
            the Balanced and Performance presets) and compares it with the High capture.
            Startup readiness is process start to the start of frame 3 (frame 2 waits for
            queued IBL work since P7.1); the median of --runs runs is reported. With
            --baseline, the High capture must be byte-identical to the baseline binary's.
  sun       The same comparison under the bright-sun HDR (--env).
  grid      Screen-ray vs plane-mesh grid at grazing, below-plane, close, far and
            orthographic views.

Needs numpy and Pillow.
"""

import argparse
import json
import math
import statistics
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'build' / 'p7'
EXE = ROOT / 'build' / 'bin' / 'Release' / 'Glider.exe'
COMMON = ['--isolated', '--window-size', '1600x900', '--vsync', 'off', '--disable-idle-wait']
VIEW = ['--camera-target', '0,1.6,0', '--camera-distance', '7.5', '--camera-yaw', '0', '--camera-pitch', '8']

QUALITY_VARIANTS = [
    ('irr64', ['--ibl-irradiance-size', '64']),
    ('irr32', ['--ibl-irradiance-size', '32']),
    ('lut512', ['--ibl-lut-size', '512']),
    ('lut256', ['--ibl-lut-size', '256']),
    ('half', ['--ibl-precision', '16']),
    ('samples512', ['--ibl-samples', '512']),
    ('samples256', ['--ibl-samples', '256']),
    ('cube512', ['--ibl-size', '512']),
    ('gridplane', ['--grid-mode', 'plane']),
    ('balanced', ['--quality', 'balanced']),
    ('performance', ['--quality', 'performance']),
]

GRID_VIEWS = {
    'graze': ['--camera-target', '0,0.3,0', '--camera-distance', '6', '--camera-yaw', '20', '--camera-pitch', '1'],
    'below': ['--camera-target', '0,0,0', '--camera-distance', '6', '--camera-yaw', '30', '--camera-pitch', '-20'],
    'close': ['--camera-target', '0,0,0', '--camera-distance', '0.3', '--camera-yaw', '10', '--camera-pitch', '30'],
    'far': ['--camera-target', '0,0,0', '--camera-distance', '400', '--camera-yaw', '10', '--camera-pitch', '25'],
    'far2': ['--camera-target', '0,0,0', '--camera-distance', '3000', '--camera-yaw', '10', '--camera-pitch', '5'],
    'ortho': ['--camera-target', '0,1,0', '--camera-distance', '8', '--camera-yaw', '35', '--camera-pitch', '30',
              '--camera-projection', 'orthographic'],
}


def make_spheres(path):
    seg, rings = 64, 48
    pos, nrm, idx = [], [], []
    for r in range(rings + 1):
        th = r / rings * math.pi
        for s in range(seg + 1):
            ph = s / seg * 2 * math.pi
            x, y, z = math.sin(th) * math.cos(ph), math.cos(th), math.sin(th) * math.sin(ph)
            pos += [x * 0.45, y * 0.45, z * 0.45]
            nrm += [x, y, z]
    for r in range(rings):
        for s in range(seg):
            a = r * (seg + 1) + s
            b = a + seg + 1
            idx += [a, a + 1, b, a + 1, b + 1, b]
    blob = struct.pack('<%df' % len(pos), *pos) + struct.pack('<%df' % len(nrm), *nrm) + struct.pack('<%dI' % len(idx), *idx)
    plen, nlen = len(pos) * 4, len(nrm) * 4
    materials, meshes, nodes = [], [], []
    for row in range(3):
        for col, rough in enumerate([0.0, 0.2, 0.4, 0.6, 0.8, 1.0]):
            if row == 0:
                m = {'pbrMetallicRoughness': {'baseColorFactor': [0.95, 0.93, 0.88, 1], 'metallicFactor': 1.0, 'roughnessFactor': rough}}
            elif row == 1:
                m = {'pbrMetallicRoughness': {'baseColorFactor': [0.9, 0.9, 0.9, 1], 'metallicFactor': 0.0, 'roughnessFactor': rough}}
            else:
                m = {'pbrMetallicRoughness': {'baseColorFactor': [0.05, 0.05, 0.08, 1], 'metallicFactor': 0.0, 'roughnessFactor': 0.8},
                     'extensions': {'KHR_materials_sheen': {'sheenColorFactor': [1.0, 0.85, 0.7], 'sheenRoughnessFactor': max(rough, 0.05)}}}
            m['name'] = 'row%d_r%.1f' % (row, rough)
            materials.append(m)
            meshes.append({'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1}, 'indices': 2, 'material': len(materials) - 1}]})
            nodes.append({'mesh': len(meshes) - 1, 'translation': [(col - 2.5) * 1.1, 0.5 + (2 - row) * 1.1, 0.0], 'name': m['name']})
    gltf = {
        'asset': {'version': '2.0', 'generator': 'validate_p7'},
        'extensionsUsed': ['KHR_materials_sheen'],
        'scene': 0, 'scenes': [{'nodes': list(range(len(nodes)))}],
        'nodes': nodes, 'meshes': meshes, 'materials': materials,
        'buffers': [{'byteLength': len(blob)}],
        'bufferViews': [
            {'buffer': 0, 'byteOffset': 0, 'byteLength': plen, 'target': 34962},
            {'buffer': 0, 'byteOffset': plen, 'byteLength': nlen, 'target': 34962},
            {'buffer': 0, 'byteOffset': plen + nlen, 'byteLength': len(idx) * 4, 'target': 34963},
        ],
        'accessors': [
            {'bufferView': 0, 'componentType': 5126, 'count': len(pos) // 3, 'type': 'VEC3', 'min': [-0.45] * 3, 'max': [0.45] * 3},
            {'bufferView': 1, 'componentType': 5126, 'count': len(pos) // 3, 'type': 'VEC3'},
            {'bufferView': 2, 'componentType': 5125, 'count': len(idx), 'type': 'SCALAR'},
        ],
    }
    js = json.dumps(gltf).encode()
    js += b' ' * ((4 - len(js) % 4) % 4)
    out = struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(blob))
    out += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(blob), 0x004E4942) + blob
    path.write_bytes(out)


def make_sun_hdr(path, width=1024, height=512):
    th = ((np.arange(height) + 0.5) / height * math.pi)[:, None]
    ph = ((np.arange(width) + 0.5) / width * 2 * math.pi)[None, :]
    up = np.cos(th) * np.ones_like(ph)
    sky = np.zeros((height, width, 3))
    sky[..., 0] = np.where(up > 0, 0.4 + 0.6 * (1 - up), 0.25)
    sky[..., 1] = np.where(up > 0, 0.6 + 0.4 * (1 - up), 0.22)
    sky[..., 2] = np.where(up > 0, 1.0, 0.2)
    el = math.radians(35)
    sun = np.array([0.0, math.sin(el), math.cos(el)])
    cosang = np.sin(th) * np.cos(ph) * sun[0] + np.cos(th) * sun[1] + np.sin(th) * np.sin(ph) * sun[2]
    sky[cosang > math.cos(math.radians(1.0))] = [2.0e5, 1.9e5, 1.7e5]
    m = sky.max(axis=2)
    mant, exp = np.frexp(m)
    scale = np.where(m > 1e-32, mant * 256.0 / np.maximum(m, 1e-32), 0)
    rgbe = np.zeros((height, width, 4), np.uint8)
    rgbe[..., :3] = np.clip(sky * scale[..., None], 0, 255).astype(np.uint8)
    rgbe[..., 3] = np.where(m > 1e-32, exp + 128, 0).astype(np.uint8)
    path.write_bytes(b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n' % (height, width) + rgbe.tobytes())


def capture(exe, name, extra, model, view=VIEW):
    png, rep = OUT / (name + '.png'), OUT / (name + '.json')
    args = [str(exe), str(model)] + COMMON + view + ['--capture-viewport', str(png), '--capture-frame', '8',
                                                     '--profile-report', str(rep), '--exit-after-profile'] + extra
    result = subprocess.run(['cmd', '/c'] + args, capture_output=True, text=True, timeout=300)
    if result.returncode != 0 or not png.exists():
        sys.exit('capture failed: %s\n%s' % (name, result.stderr[-2000:]))
    report = json.loads(rep.read_text())
    ready = report['startup']['totalStartupMs'] + sum(f['frameIntervalMs'] for f in report['frames'][1:3])
    return ready, report['startup']['counters']['textureAllocationBytes'] / 2**20


def compare(ref, img):
    a = np.asarray(Image.open(ref).convert('RGB')).astype(np.float64)
    b = np.asarray(Image.open(img).convert('RGB')).astype(np.float64)
    if a.shape != b.shape:
        return {'size': '%dx%d' % (b.shape[1], b.shape[0])}
    d = np.abs(a - b)
    mse = (d ** 2).mean()
    return {'max': int(d.max()), 'mean': round(float(d.mean()), 4),
            'psnr': None if mse == 0 else round(10 * math.log10(255 ** 2 / mse), 2),
            'pct>2': round(float((d.max(axis=2) > 2).mean() * 100), 3),
            'pct>8': round(float((d.max(axis=2) > 8).mean() * 100), 3)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', help='binary before Phase 7; its High capture must match byte for byte')
    parser.add_argument('--runs', type=int, default=3)
    parser.add_argument('--sections', default='quality,sun,grid')
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    model, hdr = OUT / 'p7_spheres.glb', OUT / 'p7_sun.hdr'
    make_spheres(model)
    make_sun_hdr(hdr)
    sections = args.sections.split(',')
    failed = False

    if 'quality' in sections:
        def timed(name, exe, extra):
            runs = [capture(exe, name if r == 0 else '%s_r%d' % (name, r), extra, model) for r in range(args.runs)]
            return round(statistics.median(r[0] for r in runs), 1), round(runs[0][1], 1)
        ready, alloc = timed('high', EXE, [])
        print('high', {'readyMs': ready, 'texAllocMB': alloc})
        if args.baseline:
            ready, alloc = timed('baseline', Path(args.baseline), [])
            diff = compare(OUT / 'high.png', OUT / 'baseline.png')
            print('baseline', {'readyMs': ready, 'texAllocMB': alloc, **diff})
            failed |= diff.get('max', 1) != 0
        for name, extra in QUALITY_VARIANTS:
            ready, alloc = timed(name, EXE, extra)
            print(name, {'readyMs': ready, 'texAllocMB': alloc, **compare(OUT / 'high.png', OUT / (name + '.png'))})

    if 'sun' in sections:
        env = ['--env', str(hdr)]
        capture(EXE, 'sun_high', env, model)
        for name, extra in [('half', ['--ibl-precision', '16']), ('irr64', ['--ibl-irradiance-size', '64']),
                            ('lut512', ['--ibl-lut-size', '512']), ('lut256', ['--ibl-lut-size', '256']),
                            ('samples512', ['--ibl-samples', '512']), ('balanced', ['--quality', 'balanced'])]:
            capture(EXE, 'sun_' + name, extra + env, model)
            print('sun_' + name, compare(OUT / 'sun_high.png', OUT / ('sun_%s.png' % name)))
        for name in ('high', 'half', 'balanced'):
            px = np.asarray(Image.open(OUT / ('sun_%s.png' % name)).convert('RGB'))
            black = int((px.sum(axis=2) == 0).sum())
            print('sun_%s black pixels' % name, black)
            failed |= black != 0

    if 'grid' in sections:
        for view, camera in GRID_VIEWS.items():
            capture(EXE, 'grid_ray_' + view, [], model, camera)
            capture(EXE, 'grid_plane_' + view, ['--grid-mode', 'plane'], model, camera)
            print('grid', view, compare(OUT / ('grid_ray_%s.png' % view), OUT / ('grid_plane_%s.png' % view)))

    print('FAIL' if failed else 'DONE (image differences are reported for review, not judged)')
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
