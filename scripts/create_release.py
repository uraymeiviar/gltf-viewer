import subprocess
import urllib.request
import urllib.error
import json
import os

def main():
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

    # Check if release exists
    release = None
    try:
        req = urllib.request.Request(f'https://api.github.com/repos/{repo}/releases/tags/v2.3.0', headers=headers)
        with urllib.request.urlopen(req) as resp:
            release = json.loads(resp.read().decode('utf-8'))
    except urllib.error.HTTPError as e:
        if e.code != 404:
            raise

    if not release:
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

    # Delete existing asset with same name if present
    assets_req = urllib.request.Request(f'https://api.github.com/repos/{repo}/releases/{release["id"]}/assets', headers=headers)
    with urllib.request.urlopen(assets_req) as resp:
        assets = json.loads(resp.read().decode('utf-8'))

    for a in assets:
        if a['name'] == 'Gestaltor-v2.3.0-win64.zip':
            del_req = urllib.request.Request(a['url'], headers=headers, method='DELETE')
            with urllib.request.urlopen(del_req) as del_resp:
                print(f'Deleted old asset: {a["id"]}')

    # Upload updated zip
    zip_file = 'build/Gestaltor-v2.3.0-win64.zip'
    with open(zip_file, 'rb') as f:
        zip_bytes = f.read()

    upload_url = release['upload_url'].split('{')[0]
    upload_headers = dict(headers)
    upload_headers['Content-Type'] = 'application/zip'
    upload_req = urllib.request.Request(
        f'{upload_url}?name=Gestaltor-v2.3.0-win64.zip',
        data=zip_bytes,
        headers=upload_headers
    )

    with urllib.request.urlopen(upload_req) as resp:
        asset_data = json.loads(resp.read().decode('utf-8'))

    print(f'Successfully uploaded release asset: {asset_data["browser_download_url"]} ({len(zip_bytes):,} bytes)')

if __name__ == '__main__':
    main()
