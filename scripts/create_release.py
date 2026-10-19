import subprocess
import urllib.request
import urllib.error
import json
import os
import zipfile

VERSION = 'v2.4.0'
RELEASE_NAME = f'Gestaltor Viewer {VERSION} (Standalone Win64)'
ZIP_NAME = f'Gestaltor-{VERSION}-win64.zip'

RELEASE_BODY = f'''### Gestaltor glTF Viewer {VERSION}

A lightweight, high-performance, 100% standalone **glTF 2.0 Visual Inspector & Viewer** based on UX3D Gestaltor.

#### Highlights in {VERSION}
- **Built-in In-Process glTF 2.0 Validator**:
  - High-performance, native C++20 validation engine running entirely in-process with zero external subprocesses or tools needed.
  - **Tier 1 (JSON Schema Validation)**: Evaluates loaded assets against official Khronos glTF 2.0 and extension schemas embedded inside the executable.
  - **Tier 2 (Semantic & Buffer Integrity Checks)**: Verifies buffer view alignments, accessor strides/bounds, min/max values, geometry attribute parity, vertex index bounds, degenerate triangles, animation monotonicity, and scene hierarchy cycle detection.
- **Interactive Validation UI Panel**:
  - Filterable issues table with Errors (🔴), Warnings (🟡), and Infos (🔵), plus real-time text search.
  - Single-click selection jumps to the offending element in the Element/Material Inspector; double-click frames it in the 3D viewport (`F`).
  - One-click "Copy Report" button generates a complete GitHub-flavored Markdown diagnostic report to clipboard.
  - Viewport toolbar status pill indicator (`[ ✓ glTF Valid ]` or `[ 🔴 N Errors ]`).
  - Global shortcut `Ctrl + 8` or **View -> Validation**.
- **Modernized Concurrency**:
  - Removed legacy Intel oneTBB runtime dependency in favor of clean standard C++20 multithreading.
- **100% Standalone Single Binary**:
  - All runtime assets (GLSL shaders, IBL HDR environment map, validation schemas, fonts with embedded Material Design Icons, sample models) are compressed with Zstandard and embedded directly inside `Gestaltor.exe`.
- **Zero Qt Dependency**: Lightweight Dear ImGui (Docking) and GLFW with OpenGL 4.1 Core Profile.
- **FSS Aviation Dark Theme**: Aviation dark theme with emerald green accents and crisp typography.
- **Blender-Style Viewport Navigation**: Orbit, pan, zoom, and frame focus (`F`) controls.
- **Native GPU Color Picking**: Subpixel mesh picking, Oriented Bounding Box (OBB) local highlighting, and coordinate gizmos.
- **Full glTF 2.0 PBR Pipeline**: Core PBR plus Specular-Glossiness, Clearcoat, Sheen, Transmission, Volume, IOR, Iridescence, Material Variants, and Punctual Lights.

#### Assets
- **`{ZIP_NAME}`**: Standalone Windows 64-bit binary. Simply extract and run!
'''

def package_zip(zip_path):
    print(f'Packaging {zip_path}...')
    exe_path = os.path.join('build', 'bin', 'Release', 'Gestaltor.exe')
    if not os.path.exists(exe_path):
        raise FileNotFoundError(f'Executable not found at {exe_path}. Build Release target first.')

    files_to_pack = [
        (exe_path, 'Gestaltor.exe'),
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
        'User-Agent': 'GestaltorReleaseScript'
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
