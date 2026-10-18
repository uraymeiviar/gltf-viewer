import subprocess
import urllib.request
import json
import os

def main():
    # Retrieve token from git credential manager
    proc = subprocess.run(['git', 'credential', 'fill'], input='protocol=https\nhost=github.com\n', text=True, capture_output=True)
    token = ''
    for line in proc.stdout.splitlines():
        if line.startswith('password='):
            token = line.split('password=')[1].strip()

    if not token:
        print('Error: Failed to retrieve token from git credential manager')
        return

    repo = 'uraymeiviar/gltf-viewer'
    release_payload = {
        'tag_name': 'v2.3.0',
        'name': 'Gestaltor Viewer v2.3.0 (Standalone Win64)',
        'body': '''### Gestaltor glTF Viewer v2.3.0

A lightweight, modern, 100% standalone **glTF 2.0 Visual Inspector & Viewer** based on UX3D Gestaltor.

#### Highlights
- **100% Standalone Single Binary**: All runtime assets (GLSL shaders, IBL HDR environment map, fonts with embedded Material Design Icons, sample models) are compressed with Zstandard and embedded directly inside `Gestaltor.exe`. No external files, DLLs, or folders required!
- **Zero Qt Dependency**: Replaced heavyweight Qt dependency with Dear ImGui (Docking) and GLFW.
- **FSS Aviation Dark Theme**: Aviation dark theme with emerald green accents, crisp typography, and Material Design icons.
- **Blender-Style Viewport Navigation**: Orbit, pan, zoom, and frame focus (`F`) controls.
- **Native GPU Color Picking**: Subpixel mesh picking, Oriented Bounding Box (OBB) local highlighting, and coordinate gizmos.
- **Full glTF 2.0 PBR Pipeline**: Core PBR plus Specular-Glossiness, Clearcoat, Sheen, Transmission, Volume, IOR, Iridescence, Material Variants, and Punctual Lights.

#### Assets
- **`Gestaltor-v2.3.0-win64.zip`**: Standalone Windows 64-bit binary. Simply extract and run!
''',
        'draft': False,
        'prerelease': False
    }

    req = urllib.request.Request(
        f'https://api.github.com/repos/{repo}/releases',
        data=json.dumps(release_payload).encode('utf-8'),
        headers={
            'Authorization': f'Bearer {token}',
            'Accept': 'application/vnd.github+json',
            'User-Agent': 'GestaltorReleaseScript'
        }
    )

    with urllib.request.urlopen(req) as resp:
        rel_data = json.loads(resp.read().decode('utf-8'))

    release_id = rel_data['id']
    html_url = rel_data['html_url']
    upload_url = rel_data['upload_url'].split('{')[0]
    print(f'Successfully created release {release_id}: {html_url}')

    # Upload zip asset
    zip_file = 'build/Gestaltor-v2.3.0-win64.zip'
    with open(zip_file, 'rb') as f:
        zip_bytes = f.read()

    upload_req = urllib.request.Request(
        f'{upload_url}?name=Gestaltor-v2.3.0-win64.zip',
        data=zip_bytes,
        headers={
            'Authorization': f'Bearer {token}',
            'Accept': 'application/vnd.github+json',
            'User-Agent': 'GestaltorReleaseScript',
            'Content-Type': 'application/zip'
        }
    )

    with urllib.request.urlopen(upload_req) as resp:
        asset_data = json.loads(resp.read().decode('utf-8'))

    download_url = asset_data['browser_download_url']
    print(f'Successfully uploaded release asset: {download_url} ({len(zip_bytes):,} bytes)')

if __name__ == '__main__':
    main()
