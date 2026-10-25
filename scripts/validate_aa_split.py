"""Targeted independent AA, legacy baseline, glass/decal and overlay checks.

Run with Anaconda Python. No full regression suite or benchmark is invoked.
All legacy captures are checked byte-for-byte, including TAA. A deliberate TAA
overlay correction can fail that check; the pixel residual is reported explicitly.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image
import harness_process as subprocess
from validate_asobo_glass import fixture, GLASS

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / 'build/review'
EXE = REPO / 'build/bin/Release/Glider.exe'
BASE = REPO / 'build/baselines/5a36073.exe'
COCKPIT = Path(r'F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf')
COMMON = ['--isolated', '--window-size', '1000x800', '--fixed-frame-time', '0.05', '--vsync', 'off']
CAMERA = ['--camera-target', '0,0,0', '--camera-yaw', '0', '--camera-pitch', '0', '--camera-distance', '4', '--exposure', '-3']
COMBOS = {'4_taa': ['--msaa', '4', '--post-aa', 'taa'],
          '4_fxaa': ['--msaa', '4', '--post-aa', 'fxaa'],
          '8_taa': ['--msaa', '8', '--post-aa', 'taa']}
results, failures = {}, []


def check(ok, label):
    print(('PASS ' if ok else 'FAIL ') + label, flush=True)
    if not ok:
        failures.append(label)


def render(model, name, flags, exe=EXE, frame=8):
    png = OUT / f'aa_{name}.png'
    png.unlink(missing_ok=True)
    args = [str(exe), str(model), *COMMON, *flags, '--capture-viewport', str(png),
            '--capture-frame', str(frame), '--profile-report', str(png.with_suffix('.json')),
            '--exit-after-profile']
    run = subprocess.run(args, cwd=REPO, capture_output=True, timeout=600)
    log = (run.stdout + run.stderr).decode('utf-8', errors='replace')
    png.with_suffix('.log').write_text(log, encoding='utf-8')
    check(run.returncode == 0 and png.exists(), name + ': capture')
    if exe == EXE:
        check('GL automation errors: 0' in log and 'Glass copy fallback:' not in log,
              name + ': zero GL errors, no glass fallback')
    if not png.exists():
        raise RuntimeError(name + ': no capture; see log')
    return png


def rgb(p):
    return np.asarray(Image.open(p).convert('RGB'))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture-only', action='store_true', help='Skip the slower E195 captures')
    options = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    model = fixture('aa_split', alignment=True)
    doc = json.loads(model.read_text())
    doc['nodes'][0]['name'] = 'SelectedGlass'
    model.write_text(json.dumps(doc))
    searches = [v for path in sorted(COCKPIT.parent.parent.glob('texture*')) if path.is_dir()
                for v in ('--search-path', str(path))]
    check(COCKPIT.exists(), 'E195 cockpit available')
    check(bool(searches), 'E195 texture search paths found')
    for label, asset, extra in [('fixture', model, CAMERA), ('cockpit', COCKPIT, searches)]:
        if label == 'cockpit' and options.fixture_only:
            continue
        if not asset.exists():
            continue
        for mode in ('none', 'msaa4', 'fxaa', 'taa'):
            current = render(asset, f'{label}_{mode}', ['--aa', mode, *extra])
            baseline = render(asset, f'{label}_{mode}_baseline', ['--aa', mode, *extra], BASE)
            a, b = rgb(current).astype(int), rgb(baseline).astype(int)
            identical = current.read_bytes() == baseline.read_bytes()
            results[f'{label}_{mode}'] = {'byte_identical': identical,
                'changed_pixels': int(np.any(a != b, axis=2).sum()),
                'mean_absolute_delta': float(np.abs(a-b).mean())}
            check(identical, f'{label} legacy {mode}: byte-identical to 5a36073')
            if not identical:
                print(f'INFO baseline residual: {results[f"{label}_{mode}"]}', flush=True)
        for combo, flags in COMBOS.items():
            image = render(asset, f'{label}_{combo}', [*extra, *flags])
            a = rgb(image)
            check(a.std() > 5 and a.mean() > 10, f'{label} {combo}: nonblack image')
            if label == 'fixture':
                disabled = render(asset, f'{label}_{combo}_glass_off', [*extra, *flags, '--asobo-disable', GLASS])
                h, w = a.shape[:2]
                crop = np.s_[h//2-60:h//2+60, w//2-60:w//2+60]
                on, off = a[crop].astype(float), rgb(disabled)[crop].astype(float)
                costs = {(dx,dy):float(np.abs(on[4:-4,4:-4] - off[4+dy:116+dy,4+dx:116+dx]).mean())
                         for dy in range(-3,4) for dx in range(-3,4)}
                shift = min(costs, key=costs.get)
                mae = float(np.abs(on-off).mean())
                green = int(((on[:,:,1] > on[:,:,0]+20) & (on[:,:,1] > on[:,:,2]+20)).sum())
                results['glass_'+combo] = {'mae': mae, 'best_shift': shift, 'decal_green_pixels': green}
                check(shift == (0,0) and mae < 1.5, f'{combo}: neutral glass copy aligned, MAE {mae:.6f}')
                check(green > 50, combo + ': green decal visible through glass')
    # Exercise the remaining combinations, including default TAA with --msaa alone.
    for samples, post in [(0,'none'),(0,'fxaa'),(0,'taa'),(2,'none'),(2,'fxaa'),(2,'taa'),(4,'none'),(8,'none'),(8,'fxaa')]:
        render(model, f'matrix_{samples}_{post}', [*CAMERA, '--msaa', str(samples), '--post-aa', post])
    default = render(model, 'default', CAMERA)
    check(default.read_bytes() == (OUT/'aa_fixture_taa.png').read_bytes(), 'default is Off + TAA')
    lit = fixture('aa_split_lit', normal=(0.7071, 0, 0.7071))
    reference = render(lit, 'lit_taa', [*CAMERA, '--aa', 'taa'])
    for combo, flags in COMBOS.items():
        image = render(lit, 'lit_'+combo, [*CAMERA, *flags])
        a = np.asarray(Image.open(image).convert('RGBA'))
        b = rgb(reference)
        h,w = a.shape[:2]
        crop = np.s_[h//2-60:h//2+60,w//2-60:w//2+60]
        mae = float(np.abs(a[crop][:,:,:3].astype(float)-b[crop]).mean())
        results['lit_'+combo] = {'mae_from_taa':mae, 'min_alpha':int(a[crop][:,:,3].min())}
        check(mae < 2 and a[crop][:,:,3].min() == 255, combo+': lit glass visible and opaque')
    # Exercise order-independent precedence, against the same resolved combination.
    order = render(model, 'precedence', [*CAMERA, '--msaa', '4', '--post-aa', 'taa', '--aa', 'none'])
    check(order.read_bytes() == (OUT/'aa_fixture_4_taa.png').read_bytes(), 'new flags override later legacy --aa')
    # Capture consecutive actual window frames so the ImGui coordinate gizmo is included.
    for mode, flags in [('taa', ['--aa', 'taa']), ('4_taa', COMBOS['4_taa'])]:
        first, second = (OUT/f'aa_overlay_{mode}_{i}.png' for i in (8,9))
        run = subprocess.run([str(EXE), str(model), *COMMON, *CAMERA, *flags,
            '--select-node', 'SelectedGlass', '--disable-redraw-skip',
            '--profile-report', str(first.with_suffix('.json')),
            '--dump-frame', str(first), '--dump-next-frame', str(second), '--exit-after-dump'],
            cwd=REPO, capture_output=True, timeout=300)
        first.with_suffix('.log').write_bytes(run.stdout+run.stderr)
        check(run.returncode == 0 and first.exists() and second.exists(), mode + ': consecutive window captures')
        a,b = rgb(first),rgb(second)
        # Saturated box and axis core pixels; scene decal is deliberately excluded
        # by the exact palette. Anti-aliased fringes can blend with evolving scene.
        palettes = {'bbox': [(255,158,31)], 'gizmo': [(235,45,45),(45,215,45),(45,120,245)]}
        for kind, colors in palettes.items():
            masks = [np.any(np.stack([np.all(im == c,axis=2) for c in colors]), axis=0) for im in (a,b)]
            count, changed = int(masks[0].sum()), int(np.count_nonzero(masks[0] != masks[1]))
            results[f'overlay_{mode}_{kind}'] = {'core_pixels':count, 'changed_mask_pixels':changed}
            check(count > 0 and changed == 0, f'{mode} {kind}: {count} core pixels, {changed} changed between frames 8/9')
        results['overlay_'+mode+'_full'] = {'changed_pixels':int(np.any(a!=b,axis=2).sum())}
    cube = REPO / 'resources/sampleGltf/HelloCube.glb'
    first, second = (OUT/f'aa_grid_{i}.png' for i in (8,9))
    run = subprocess.run([str(EXE), str(cube), *COMMON, '--aa', 'taa',
        '--select-node', 'Hello Cube', '--camera-target', '0,1,0', '--camera-yaw', '35',
        '--camera-pitch', '25', '--camera-distance', '6', '--disable-redraw-skip',
        '--profile-report', str(first.with_suffix('.json')),
            '--dump-frame', str(first), '--dump-next-frame', str(second), '--exit-after-dump'],
        cwd=REPO, capture_output=True, timeout=300)
    first.with_suffix('.log').write_bytes(run.stdout+run.stderr)
    check(run.returncode == 0 and first.exists() and second.exists(), 'grid: consecutive window captures')
    # Fixed isolated 1000x800 layout: empty ground below the cube, away from UI.
    a,b = rgb(first)[600:740,240:780],rgb(second)[600:740,240:780]
    residual = int(np.any(a!=b,axis=2).sum())
    results['grid'] = {'region':[240,600,780,740], 'changed_pixels':residual,
                       'max_channel_delta':int(np.abs(a.astype(int)-b.astype(int)).max())}
    check(results['grid']['max_channel_delta'] <= 1,
          f'grid: {residual}/75600 pixels differ, max {results["grid"]["max_channel_delta"]}/255 between frames 8/9')
    (OUT/'aa_results.json').write_text(json.dumps({'results':results,'failures':failures},indent=2))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
