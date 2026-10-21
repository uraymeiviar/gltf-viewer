#!/usr/bin/env python3
"""Build a Glider executable from the accepted baseline commit plus the UI scenario harness.

The baseline sources are taken from git (default 009c062) and receive only the
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
    "source/imgui/ResourcesPanel.cpp",
    "source/imgui/ValidationPanel.cpp",
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
    describe = slice_between(candidate, "static std::string describeUiSelection", "int main(")
    config = slice_between(candidate, "    if (!uiScriptPath.empty()) {", "    double lastFrameTime")
    end_hook = slice_between(candidate, "        if (uiScenario.active()) {\n            ux3d::imgui::UiScenarioFrame", "        glfwSwapBuffers(window);")
    args_block = slice_between(candidate, '        } else if (arg == "--ui-script"', '        } else if (arg == "--isolated"')

    text = base
    text = replace_once(text, "int main(", describe + "int main(", "describe")
    text = replace_once(text, '    std::string dumpFramePath = "";\n',
                        '    std::string dumpFramePath = "";\n    std::string uiScriptPath;\n    std::string uiReportPath;\n    ux3d::imgui::UiScenario uiScenario;\n', "decl")
    text = replace_once(text, '        } else if (arg == "--isolated" || arg == "--no-persist") {\n            s_isolatedState',
                        args_block + '        } else if (arg == "--isolated" || arg == "--no-persist") {\n            s_isolatedState', "args")
    text = replace_once(text, "    double lastFrameTime = glfwGetTime();", config + "    double lastFrameTime = glfwGetTime();", "config")
    text = replace_once(text, "        ImGui::NewFrame();",
                        "        if (uiScenario.active()) {\n            uiScenario.beginFrame(ImGui::GetIO());\n        }\n        ImGui::NewFrame();", "newframe")
    text = replace_once(text, "        glfwSwapBuffers(window);", end_hook + "        glfwSwapBuffers(window);", "swap")
    text = '#include "imgui/UiScenarioHarness.h"\n' + text

    text = replace_once(
        text, '                if (ImGui::Button(ICON_MDI_DELETE_OUTLINE " Clear")) {\n',
        '                const bool loggerClearClicked = ImGui::Button(ICON_MDI_DELETE_OUTLINE " Clear");\n'
        '                ux3d::imgui::UiProbe::item("log.clear", 0);\n'
        '                if (loggerClearClicked) {\n', "clear")
    text = replace_once(
        text, '                if (ImGui::Button(ICON_MDI_CONTENT_COPY " Copy")) {\n',
        '                const bool loggerCopyClicked = ImGui::Button(ICON_MDI_CONTENT_COPY " Copy");\n'
        '                ux3d::imgui::UiProbe::item("log.copy", 0);\n'
        '                if (loggerCopyClicked) {\n', "copy")
    text = replace_once(
        text, '                ImGui::Checkbox(ICON_MDI_WRAP " Wrap", &s_loggerWrap);\n',
        '                ImGui::Checkbox(ICON_MDI_WRAP " Wrap", &s_loggerWrap);\n'
        '                ux3d::imgui::UiProbe::item("log.wrap", 0);\n', "wrap")
    text = replace_once(
        text, '                ImGui::Checkbox("Auto-Scroll", &s_loggerAutoScroll);\n',
        '                ImGui::Checkbox("Auto-Scroll", &s_loggerAutoScroll);\n'
        '                ux3d::imgui::UiProbe::item("log.autoscroll", 0);\n', "autoscroll")
    text = replace_once(
        text, '                    ImGui::TextColored(entry.color, "%s", entry.text.c_str());\n',
        '                    ImGui::TextColored(entry.color, "%s", entry.text.c_str());\n'
        '                    ux3d::imgui::UiProbe::item("log.row", static_cast<long long>(&entry - s_consoleLogs.entries().data()));\n', "row")
    return text


def baseline_resources(base: str) -> str:
    text = eol(base)
    text = replace_once(text, "#include <imgui.h>\n", '#include <imgui.h>\n#include "imgui/UiProbe.h"\n', "include")
    text = replace_once(
        text,
        '    ImGui::InputTextWithHint("##ResourceSearch", ICON_MDI_MAGNIFY " Filter resources...", s_resourceSearchBuffer, sizeof(s_resourceSearchBuffer));\n',
        '    ImGui::InputTextWithHint("##ResourceSearch", ICON_MDI_MAGNIFY " Filter resources...", s_resourceSearchBuffer, sizeof(s_resourceSearchBuffer));\n'
        '    ux3d::imgui::UiProbe::item("res.search", 0);\n', "search")
    text = replace_once(
        text, "        if (ImGui::TreeNodeEx(cache.header.c_str(), catFlags)) {",
        "        const bool categoryOpen = ImGui::TreeNodeEx(cache.header.c_str(), catFlags);\n"
        "        ux3d::imgui::UiProbe::item(categoryTitle, -1);\n"
        "        if (categoryOpen) {", "category")
    row = '                ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(item.m_ID + idOffset)), flags, "%s", itemCache.label.c_str());\n'
    text = replace_once(text, row, row + "                ux3d::imgui::UiProbe::item(categoryTitle, static_cast<long long>(item.m_ID));\n", "row")
    text = replace_once(
        text, "        if (ImGui::TreeNodeEx(variantCache.header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {",
        "        const bool variantsOpen = ImGui::TreeNodeEx(variantCache.header.c_str(), ImGuiTreeNodeFlags_DefaultOpen);\n"
        '        ux3d::imgui::UiProbe::item("Material Variants", -1);\n'
        "        if (variantsOpen) {", "variants")
    var = '                ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(var.m_ID + 900000)), flags, "%s", variantItem.label.c_str());\n'
    text = replace_once(text, var, var + '                ux3d::imgui::UiProbe::item("Material Variants", static_cast<long long>(var.m_ID));\n', "variant row")
    return text


def baseline_validation(base: str) -> str:
    text = eol(base)
    text = replace_once(text, "#include <imgui.h>\n", '#include <imgui.h>\n#include "imgui/UiProbe.h"\n', "include")
    text = replace_once(
        text,
        '    ImGui::InputTextWithHint("##IssueSearch", "Search issues...", s_searchBuffer, sizeof(s_searchBuffer));\n',
        '    ImGui::InputTextWithHint("##IssueSearch", "Search issues...", s_searchBuffer, sizeof(s_searchBuffer));\n'
        '    ux3d::imgui::UiProbe::item("val.search", 0);\n', "search")
    text = replace_once(
        text,
        "            if (ImGui::Selectable(rowSelectableId, isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {",
        "            const bool rowClicked = ImGui::Selectable(rowSelectableId, isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);\n"
        '            ux3d::imgui::UiProbe::item("val.row", rowIdx);\n'
        "            if (rowClicked) {", "selectable")
    text = replace_once(text, "            ImGui::TextUnformatted(issue.message.c_str());\n",
                        "            ImGui::TextUnformatted(issue.message.c_str());\n"
                        '            ux3d::imgui::UiProbe::item("val.msg", rowIdx);\n', "message")
    text = replace_once(text, "                ImGui::BeginTooltip();\n",
                        "                ImGui::BeginTooltip();\n"
                        '                ux3d::imgui::UiProbe::mark("val.tooltip", rowIdx);\n', "tooltip")
    text = replace_once(text, "        ImGui::EndTable();\n",
                        '        ux3d::imgui::UiProbe::mark("val.selected", s_selectedIssueIndex);\n        ImGui::EndTable();\n', "endtable")
    return text


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--commit", default="009c062")
    parser.add_argument("--output", type=Path, default=REPO / "build" / "p2.3-baseline" / "Glider.exe")
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
            "source/imgui/ResourcesPanel.cpp": baseline_resources(git_show(args.commit, "source/imgui/ResourcesPanel.cpp")),
            "source/imgui/ValidationPanel.cpp": baseline_validation(git_show(args.commit, "source/imgui/ValidationPanel.cpp")),
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


