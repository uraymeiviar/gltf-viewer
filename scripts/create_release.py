import subprocess
import urllib.request
import urllib.error
import json
import os
import zipfile

VERSION = 'v1.0.0'
RELEASE_NAME = f'Glider {VERSION} (Standalone Win64)'
ZIP_NAME = f'Glider-{VERSION}-win64.zip'

RELEASE_BODY = f'''### Glider glTF Viewer {VERSION}

A lightweight, high-performance, 100% standalone **glTF 2.0 Visual Inspector & Viewer** based on UX3D Gestaltor.

#### Highlights in {VERSION}
- **Rebranding to Glider**:
  - Fresh modern identity with custom high-DPI application icon.
  - Dedicated user configuration directory (`%APPDATA%/Glider/`).
- **Multi-Viewport Architecture**:
  - Open multiple concurrent 3D viewports docked as tabs or side-by-side splits.
  - Viewport items in Scene Hierarchy tree with per-viewport selection and individual settings in the Inspector.
  - Attach viewports to any glTF camera node (`[ Free Look | <camera-list> ]`) or inspect with free look navigation.
  - Dedicated Viewport menu (New Viewport, Camera Attachment, Orientation Presets, Shading Mode, Grid & BBox toggles).
- **OpenEXR (.exr) & HDR Environment Maps**:
  - Full support for OpenEXR (`.exr`) panoramic environment maps via statically embedded `tinyexr`.
  - Dynamic 3-channel (`.hdr`) and 4-channel (`.exr` RGBA) floating-point IBL cubemap filtering for irradiance and GGX specular reflections.
  - Viewport Settings file dialog and `--env` / `--hdr <path>` CLI argument support.
- **Comprehensive Image & Texture Inspection**:
  - Real-time proportional image preview in Element Inspector constrained to inspector width with 1px border.
  - Exact image channel count (Grayscale, RG, RGB, RGBA) and bit depth (e.g., 8-bit, 16-bit per channel / 48-bit, 64-bit total).
- **Temporal Anti-Aliasing (TAA) & Post-Processing**:
  - Rock-solid TAA with Halton(2, 3) 8-phase subpixel jitter and 3x3 color neighborhood clamping.
  - Dynamic responsive motion blending and crisp stationary accumulation.
  - FXAA 3.11 and MSAA 2x/4x/8x multi-sample anti-aliasing options.
- **Built-in In-Process glTF 2.0 Validator**:
  - High-performance native C++20 validation engine running entirely in-process with zero external subprocesses or tools needed.
  - **Tier 1 (JSON Schema Validation)**: Evaluates loaded assets against official Khronos glTF 2.0 and extension schemas embedded inside the executable.
  - **Tier 2 (Semantic & Buffer Integrity Checks)**: Verifies buffer view alignments, accessor strides/bounds, min/max values, geometry attribute parity, vertex index bounds, degenerate triangles, animation monotonicity, and scene hierarchy cycle detection.
  - **Interactive Validation Panel**: Filterable issues table (Errors 🔴, Warnings 🟡, Infos 🔵), text search, single-click Inspector selection, double-click 3D viewport framing (`F`), and one-click GitHub-flavored Markdown report export.
  - Viewport toolbar status pill indicator (`[ ✓ glTF Valid ]` or `[ 🔴 N Errors ]`).
- **100% Standalone Single Binary**:
  - All runtime assets (GLSL shaders, IBL HDR/EXR environment map, validation schemas, fonts with embedded Material Design Icons, sample models) are compressed with Zstandard and embedded directly inside `Glider.exe`.
- **Zero Qt Dependency**: Lightweight Dear ImGui (Docking) and GLFW with OpenGL 4.1 Core Profile.
- **FSS Aviation Dark Theme**: Aviation dark theme with emerald green accents, crisp typography, and high-DPI icons.
- **Blender-Style Viewport Navigation**: Orbit, pan, zoom, frame focus (`F`), and corner orientation compass gizmo.
- **Full glTF 2.0 PBR Pipeline**: Core PBR plus Specular-Glossiness, Clearcoat, Sheen, Transmission, Volume, IOR, Iridescence, Material Variants, and Punctual Lights.

#### Assets
- **`{ZIP_NAME}`**: Standalone Windows 64-bit binary. Simply extract and run!
'''

def package_zip(zip_path):
    print(f'Packaging {zip_path}...')
    exe_path = os.path.join('build', 'bin', 'Release', 'Glider.exe')
    if not os.path.exists(exe_path):
        exe_path = os.path.join('build', 'bin', 'Release', 'Gestaltor.exe')
    if not os.path.exists(exe_path):
        raise FileNotFoundError(f'Executable not found at {exe_path}. Build Release target first.')

    files_to_pack = [
        (exe_path, 'Glider.exe'),
        ('LICENSE', 'LICENSE'),
        ('README.md', 'README.md'),
    ]

    os.makedirs(os.path.dirname(zip_path), exist_ok=True)
    with zipfile.ZipFile(zip_path, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for src, arcname in files_to_pack:
            if not os.path.exists(src):
                raise FileNotFoundError(f'File to pack not found: {src}')
            zf.write(src, arcname)
            print(f'  Added {src} -> {arcname}')

    size_mb = os.path.getsize(zip_path) / (1024 * 1024)
    print(f'Successfully created {zip_path} ({size_mb:.2f} MB)')

def main():
    # 1. Package the zip archive
    zip_path = os.path.join('build', ZIP_NAME)
    package_zip(zip_path)

    # 2. Retrieve GitHub token via Git Credential Manager
    proc = subprocess.run(['git', 'credential', 'fill'], input='protocol=https\nhost=github.com\n', text=True, capture_output=True)
    token = ''
    for line in proc.stdout.splitlines():
        if line.startswith('password='):
            token = line.split('password=')[1].strip()

    if not token:
        print('Error: Failed to retrieve token from git credential manager')
        return

    repo = 'uraymeiviar/gltf-viewer'
    headers = {
        'Authorization': f'Bearer {token}',
        'Accept': 'application/vnd.github+json',
        'User-Agent': 'GliderReleaseScript'
    }

    # 3. Check if release exists
    release = None
    try:
        req = urllib.request.Request(f'https://api.github.com/repos/{repo}/releases/tags/{VERSION}', headers=headers)
        with urllib.request.urlopen(req) as resp:
            release = json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as e:
        if e.code != 404:
            raise

    if not release:
        release_payload = {
            'tag_name': VERSION,
            'target_commitish': 'msfs-gltf',
            'name': RELEASE_NAME,
            'body': RELEASE_BODY,
            'draft': False,
            'prerelease': False
        }
        create_req = urllib.request.Request(
            f'https://api.github.com/repos/{repo}/releases',
            data=json.dumps(release_payload).encode('utf-8'),
            headers=headers
        )
        with urllib.request.urlopen(create_req) as resp:
            release = json.loads(resp.read().decode('utf-8'))
        print(f'Created release {release["id"]}: {release["html_url"]}')
    else:
        print(f'Found existing release {release["id"]}: {release["html_url"]}')

    # 4. Delete existing asset with same name if present
    assets_req = urllib.request.Request(f'https://api.github.com/repos/{repo}/releases/{release["id"]}/assets', headers=headers)
    with urllib.request.urlopen(assets_req) as resp:
        assets = json.loads(resp.read().decode('utf-8'))

    for a in assets:
        if a['name'] == ZIP_NAME:
            del_req = urllib.request.Request(a['url'], headers=headers, method='DELETE')
            with urllib.request.urlopen(del_req) as del_resp:
                print(f'Deleted old asset: {a["id"]}')

    # 5. Upload zip
    with open(zip_path, 'rb') as f:
        zip_bytes = f.read()

    upload_url = release['upload_url'].split('{')[0]
    upload_headers = dict(headers)
    upload_headers['Content-Type'] = 'application/zip'
    upload_req = urllib.request.Request(
        f'{upload_url}?name={ZIP_NAME}',
        data=zip_bytes,
        headers=upload_headers
    )

    with urllib.request.urlopen(upload_req) as resp:
        asset_data = json.loads(resp.read().decode('utf-8'))

    print(f'Successfully uploaded release asset: {asset_data["browser_download_url"]} ({len(zip_bytes):,} bytes)')

if __name__ == '__main__':
    main()
