#!/usr/bin/env python3
"""Build a Glider executable from the accepted baseline commit plus the UI scenario harness.

The baseline sources are taken from git (default fc6a89b) and receive only the
harness hooks and probe lines that already exist in the working tree. The
working-tree files are backed up, replaced, built, and restored (SHA-256
verified) so the candidate sources are never altered.
"""

import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
FILES = [
    "source/imgui/main.cpp",
    "source/imgui/SceneHierarchyPanel.cpp",
]


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def git_show(commit: str, rel: str) -> str:
    return subprocess.run(
        ["git", "show", f"{commit}:{rel}"], cwd=REPO, check=True, capture_output=True
    ).stdout.decode("utf-8")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if text.count(old) != 1:
        raise SystemExit(f"anchor '{label}' found {text.count(old)} times (expected 1)")
    return text.replace(old, new)


def slice_between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


def eol(text: str) -> str:
    return text.replace("\r\n", "\n")


def baseline_main(base: str, candidate: str) -> str:
    base, candidate = eol(base), eol(candidate)
    describe_base = slice_between(base, "static std::string describeUiSelection", "int main(")
    describe_cand = slice_between(candidate, "static std::string describeUiSelection", "int main(")
    return replace_once(base, describe_base, describe_cand, "describeUiSelection")


def baseline_hierarchy(base: str) -> str:
    text = eol(base)
    text = replace_once(text, "#include <imgui.h>\n", '#include <imgui.h>\n#include "imgui/UiProbe.h"\n', "include")
    text = replace_once(
        text,
        '    ImGui::InputTextWithHint("##HierarchySearch", ICON_MDI_MAGNIFY " Filter nodes...", s_searchBuffer, sizeof(s_searchBuffer));\n',
        '    ImGui::InputTextWithHint("##HierarchySearch", ICON_MDI_MAGNIFY " Filter nodes...", s_searchBuffer, sizeof(s_searchBuffer));\n'
        '    ux3d::imgui::UiProbe::item("hier.search", 0);\n', "search")
    text = replace_once(
        text,
        '        if (ImGui::TreeNodeEx(sceneLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {\n',
        '        const bool sceneOpen = ImGui::TreeNodeEx(sceneLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);\n'
        '        ux3d::imgui::UiProbe::item("hier.scene", static_cast<long long>(activeScene->m_ID));\n'
        '        if (sceneOpen) {\n', "scene")
    text = replace_once(
        text,
        '    bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(node->m_ID)), flags, "%s", cached->label.c_str());\n',
        '    bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(node->m_ID)), flags, "%s", cached->label.c_str());\n'
        '    ux3d::imgui::UiProbe::item("hier.node", static_cast<long long>(node->m_ID));\n', "node")
    text = replace_once(
        text,
        '            bool meshOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mesh->m_ID + 2000000)), meshFlags, "%s", cached->meshLabel.c_str());\n',
        '            bool meshOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mesh->m_ID + 2000000)), meshFlags, "%s", cached->meshLabel.c_str());\n'
        '            ux3d::imgui::UiProbe::item("hier.mesh", static_cast<long long>(mesh->m_ID));\n', "mesh")
    text = replace_once(
        text,
        '                    bool primOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mesh->m_ID * 1000 + primIdx + 3000000)), primFlags, "%s", primLabel.c_str());\n',
        '                    bool primOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mesh->m_ID * 1000 + primIdx + 3000000)), primFlags, "%s", primLabel.c_str());\n'
        '                    ux3d::imgui::UiProbe::item("hier.primitive", static_cast<long long>(primIdx));\n', "prim")
    text = replace_once(
        text,
        '                        ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mat->m_ID + 4000000)), matFlags, "%s", matLabel.c_str());\n',
        '                        ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(mat->m_ID + 4000000)), matFlags, "%s", matLabel.c_str());\n'
        '                        ux3d::imgui::UiProbe::item("hier.material", static_cast<long long>(mat->m_ID));\n', "mat")
    text = replace_once(
        text,
        '            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(cam->m_ID + 5000000)), camFlags, "%s", cached->cameraLabel.c_str());\n',
        '            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(cam->m_ID + 5000000)), camFlags, "%s", cached->cameraLabel.c_str());\n'
        '            ux3d::imgui::UiProbe::item("hier.camera", static_cast<long long>(cam->m_ID));\n', "cam")
    text = replace_once(
        text,
        '            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(light->m_ID + 6000000)), lightFlags, "%s", cached->lightLabel.c_str());\n',
        '            ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(light->m_ID + 6000000)), lightFlags, "%s", cached->lightLabel.c_str());\n'
        '            ux3d::imgui::UiProbe::item("hier.light", static_cast<long long>(light->m_ID));\n', "light")
    text = replace_once(
        text,
        '        if (ImGui::TreeNodeEx(vpHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {\n',
        '        const bool vpHeaderOpen = ImGui::TreeNodeEx(vpHeader.c_str(), ImGuiTreeNodeFlags_DefaultOpen);\n'
        '        ux3d::imgui::UiProbe::item("hier.viewports_header", 0);\n'
        '        if (vpHeaderOpen) {\n', "vpHeader")
    text = replace_once(
        text,
        '                ImGui::TreeNodeEx((void*)(intptr_t)vp->id, nodeFlags, "%s", itemLabel.c_str());\n',
        '                ImGui::TreeNodeEx((void*)(intptr_t)vp->id, nodeFlags, "%s", itemLabel.c_str());\n'
        '                ux3d::imgui::UiProbe::item("hier.viewport", static_cast<long long>(vp->id));\n', "vp")
    text = replace_once(
        text,
        '    std::string btnLabel = std::string(icon) + "##eye_n_" + std::to_string(node->m_ID);\n'
        '    if (ImGui::Button(btnLabel.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {\n',
        '    std::string btnLabel = std::string(icon) + "##eye_n_" + std::to_string(node->m_ID);\n'
        '    const bool nodeEyeClicked = ImGui::Button(btnLabel.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()));\n'
        '    ux3d::imgui::UiProbe::item("hier.node_eye", static_cast<long long>(node->m_ID));\n'
        '    if (nodeEyeClicked) {\n', "node_eye")
    text = replace_once(
        text,
        '    std::string btnLabel = std::string(icon) + "##eye_m_" + std::to_string(mesh->m_ID);\n'
        '    if (ImGui::Button(btnLabel.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {\n',
        '    std::string btnLabel = std::string(icon) + "##eye_m_" + std::to_string(mesh->m_ID);\n'
        '    const bool meshEyeClicked = ImGui::Button(btnLabel.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()));\n'
        '    ux3d::imgui::UiProbe::item("hier.mesh_eye", static_cast<long long>(mesh->m_ID));\n'
        '    if (meshEyeClicked) {\n', "mesh_eye")
    text = replace_once(
        text,
        '    if (ImGui::Button(eyeBtnId.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()))) {\n',
        '    const bool vpEyeClicked = ImGui::Button(eyeBtnId.c_str(), ImVec2(buttonWidth, ImGui::GetFrameHeight()));\n'
        '    ux3d::imgui::UiProbe::item("hier.vp_eye", static_cast<long long>(vp->id));\n'
        '    if (vpEyeClicked) {\n', "vp_eye")
    return text


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--commit", default="fc6a89b")
    parser.add_argument("--output", type=Path, default=REPO / "build" / "p2.4-baseline" / "Glider.exe")
    parser.add_argument("--backup", type=Path, required=True)
    parser.add_argument("--build-dir", type=Path, default=REPO / "build")
    args = parser.parse_args()

    args.backup.mkdir(parents=True, exist_ok=True)
    candidate = {rel: (REPO / rel).read_bytes() for rel in FILES}
    hashes = {rel: sha(REPO / rel) for rel in FILES}
    for rel in FILES:
        (args.backup / Path(rel).name).write_bytes(candidate[rel])

    status = 0
    try:
        sources = {
            "source/imgui/main.cpp": baseline_main(git_show(args.commit, "source/imgui/main.cpp"),
                                                   candidate["source/imgui/main.cpp"].decode("utf-8")),
            "source/imgui/SceneHierarchyPanel.cpp": baseline_hierarchy(git_show(args.commit, "source/imgui/SceneHierarchyPanel.cpp")),
        }
        for rel, text in sources.items():
            (REPO / rel).write_text(text, encoding="utf-8", newline="\n")
        result = subprocess.run(
            ["cmake", "--build", str(args.build_dir), "--config", "Release", "--target", "Glider"], cwd=REPO
        )
        if result.returncode != 0:
            print("baseline build failed", file=sys.stderr)
            status = 1
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(REPO / "build" / "bin" / "Release" / "Glider.exe", args.output)
            print(f"baseline harness exe: {args.output} SHA256 {sha(args.output)}")
    except SystemExit as exc:
        print(exc, file=sys.stderr)
        status = 1
    finally:
        for rel in FILES:
            (REPO / rel).write_bytes(candidate[rel])
        for rel in FILES:
            if sha(REPO / rel) != hashes[rel]:
                print(f"RESTORE MISMATCH: {rel}", file=sys.stderr)
                status = 2
    return status


if __name__ == "__main__":
    raise SystemExit(main())


