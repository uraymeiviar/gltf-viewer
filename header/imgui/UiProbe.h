#pragma once

#include <imgui.h>

#include <string>
#include <vector>

namespace ux3d::imgui {

struct UiProbeEntry {
    std::string tag;
    long long id = -1;
    ImVec2 min;
    ImVec2 max;
    bool hovered = false;
    bool focused = false;
    bool isMark = false;
};

// Records the screen rectangles of visible UI items so the in-process scenario
// harness (UiScenarioHarness.h) can drive and observe real ImGui frames. It is
// disabled unless --ui-script is passed, so normal runs pay one bool test.
class UiProbe {
public:
    static bool& enabled()
    {
        static bool value = false;
        return value;
    }

    static std::vector<UiProbeEntry>& entries()
    {
        static std::vector<UiProbeEntry> value;
        return value;
    }

    // Call immediately after the item is submitted.
    static void item(const char* tag, long long id)
    {
        if (!enabled()) {
            return;
        }
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        const ImVec2 itemMax = ImGui::GetItemRectMax();
        if (!ImGui::IsRectVisible(itemMin, itemMax)) {
            return;
        }
        entries().push_back({ tag, id, itemMin, itemMax, ImGui::IsItemHovered(), ImGui::IsItemFocused(), false });
    }

    // For content drawn directly to the draw list without submitting an item.
    static void rect(const char* tag, long long id, ImVec2 rectMin, ImVec2 rectMax)
    {
        if (!enabled() || !ImGui::IsRectVisible(rectMin, rectMax)) {
            return;
        }
        entries().push_back({ tag, id, rectMin, rectMax, false, false, false });
    }

    static void mark(const char* tag, long long id)
    {
        if (!enabled()) {
            return;
        }
        entries().push_back({ tag, id, ImVec2(), ImVec2(), false, false, true });
    }
};

} // namespace ux3d::imgui
