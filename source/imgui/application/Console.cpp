// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "Application.h"
#include "imgui/Theme.h"
#include <algorithm>
#include <cstdio>
#include <sstream>

namespace ux3d::imgui::application {

static ImVec2 measureConsoleLogText(const LogEntry& entry, ImFont* font, float fontSize, bool wrap, float wrapWidth)
{
    const char* textBegin = entry.text.c_str();
    const char* textEnd = textBegin + entry.text.size();
    return font->CalcTextSizeA(fontSize, std::numeric_limits<float>::max(), wrap ? wrapWidth : 0.0f, textBegin,
                               textEnd);
}

static bool consoleLogLayoutSettingsMatch(const ConsoleLogLayoutCache& cache, ImFont* font, float fontSize, bool wrap,
                                          float wrapWidth, float itemSpacingY)
{
    return cache.valid && cache.font == font && cache.fontSize == fontSize && cache.wrap == wrap &&
           cache.wrapWidth == wrapWidth && cache.itemSpacingY == itemSpacingY;
}

static void appendConsoleLogLayoutRows(ConsoleLogLayoutCache& cache, const std::vector<LogEntry>& entries,
                                       size_t startIndex)
{
    for (size_t i = startIndex; i < entries.size(); ++i) {
        const LogEntry& entry = entries[i];
        const ImVec2 textSize = measureConsoleLogText(entry, cache.font, cache.fontSize, cache.wrap, cache.wrapWidth);
        const float rowHeight = std::max(cache.fontSize, textSize.y);
        if (!cache.rowHeights.empty()) {
            cache.contentHeight += cache.itemSpacingY;
        }
        cache.rowOffsets.push_back(cache.contentHeight);
        cache.rowHeights.push_back(rowHeight);
        cache.rowEnds.push_back(cache.contentHeight + rowHeight);
        cache.contentHeight += rowHeight;
        cache.maxTextWidth = std::max(cache.maxTextWidth, textSize.x);
        cache.allSingleLine = cache.allSingleLine && entry.text.find_first_of("\r\n") == std::string::npos;
        ++cache.appendedRowsMeasured;
    }
}

static void ensureConsoleLogLayout(ConsoleLogLayoutCache& cache, const ConsoleLogStore& store, ImFont* font,
                                   float fontSize, bool wrap, float wrapWidth, float itemSpacingY)
{
    const auto& entries = store.entries();
    const bool sameSettings = consoleLogLayoutSettingsMatch(cache, font, fontSize, wrap, wrapWidth, itemSpacingY);
    const bool canExtend =
        sameSettings && cache.clearGeneration == store.clearGeneration() && cache.entryCount <= entries.size();
    const bool contentChanged = !cache.valid || cache.revision != store.revision() ||
                                cache.entryCount != entries.size() || !sameSettings ||
                                cache.clearGeneration != store.clearGeneration();

    if (!contentChanged) {
        return;
    }

    if (canExtend) {
        appendConsoleLogLayoutRows(cache, entries, cache.entryCount);
    } else {
        cache.valid = true;
        cache.font = font;
        cache.fontSize = fontSize;
        cache.wrap = wrap;
        cache.wrapWidth = wrapWidth;
        cache.itemSpacingY = itemSpacingY;
        cache.allSingleLine = true;
        cache.maxTextWidth = 0.0f;
        cache.contentHeight = 0.0f;
        cache.rowOffsets.clear();
        cache.rowHeights.clear();
        cache.rowEnds.clear();
        ++cache.fullRebuilds;
        appendConsoleLogLayoutRows(cache, entries, 0);
    }

    cache.revision = store.revision();
    cache.clearGeneration = store.clearGeneration();
    cache.entryCount = entries.size();
}

static std::pair<size_t, size_t> visibleConsoleLogRows(const ConsoleLogLayoutCache& cache, float scrollY,
                                                       float viewportHeight)
{
    if (cache.rowEnds.empty() || viewportHeight <= 0.0f) {
        return {0, 0};
    }

    const auto first = std::upper_bound(cache.rowEnds.begin(), cache.rowEnds.end(), scrollY);
    const auto last = std::lower_bound(cache.rowOffsets.begin(), cache.rowOffsets.end(), scrollY + viewportHeight);
    return {static_cast<size_t>(std::distance(cache.rowEnds.begin(), first)),
            static_cast<size_t>(std::distance(cache.rowOffsets.begin(), last))};
}

static std::string buildConsoleLogCopyText(const std::vector<LogEntry>& entries)
{
    std::string allLogs;
    for (const auto& entry : entries) {
        allLogs += entry.text;
        allLogs += "\n";
    }
    return allLogs;
}

ux3d::imgui::SelfCheckResult Application::runConsoleLogLayoutSelfCheck(bool forceFailure)
{
    ux3d::imgui::SelfCheckResult result;
    int checks = 0;
    std::vector<std::string> failures;
    const auto check = [&](bool condition, const std::string& message) {
        ++checks;
        if (!condition && failures.size() < 20) {
            failures.push_back(message);
        }
    };

    ConsoleLogStore store;
    store.append({"single row", ImVec4(1, 1, 1, 1), ux3d::core::LogLevel::Info});
    store.append({"first line\nsecond line", ImVec4(1, 1, 1, 1), ux3d::core::LogLevel::Warning});
    store.append({std::string(160, 'W'), ImVec4(1, 1, 1, 1), ux3d::core::LogLevel::Error});
    check(store.counts().info == 1 && store.counts().warnings == 1 && store.counts().errors == 1,
          "append did not preserve severity totals");
    check(buildConsoleLogCopyText(store.entries()) ==
              "single row\nfirst line\nsecond line\n" + std::string(160, 'W') + "\n",
          "Copy text omitted or changed a stored entry");

    ConsoleLogLayoutCache layout;
    ImFont* monoFont = ux3d::imgui::getMonoFont();
    ImGui::PushFont(monoFont);
    const float fontSize = ImGui::GetFontSize();
    const float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
    ensureConsoleLogLayout(layout, store, monoFont, fontSize, false, 0.0f, itemSpacingY);
    check(layout.rowHeights.size() == 3 && !layout.allSingleLine,
          "unwrapped multiline entries were not classified as variable-height");
    check(layout.rowHeights.size() == 3 && layout.rowHeights[1] > layout.rowHeights[0],
          "embedded newline did not increase the cached row height");
    check(layout.maxTextWidth > fontSize * 80.0f, "content-width cache omitted a long off-screen log entry");

    const size_t rebuildsBeforeWarmLayout = layout.fullRebuilds;
    ensureConsoleLogLayout(layout, store, monoFont, fontSize, false, 0.0f, itemSpacingY);
    check(layout.fullRebuilds == rebuildsBeforeWarmLayout, "unchanged logger content rebuilt its warm layout");

    const size_t rebuildsBeforeUnwrappedResize = layout.fullRebuilds;
    ensureConsoleLogLayout(layout, store, monoFont, fontSize, false, 500.0f, itemSpacingY);
    check(layout.wrapWidth == 500.0f && layout.fullRebuilds == rebuildsBeforeUnwrappedResize + 1,
          "available-width change did not invalidate the unwrapped layout");

    if (layout.rowOffsets.size() == 3 && layout.rowHeights.size() == 3) {
        const auto top = visibleConsoleLogRows(layout, 0.0f, layout.rowHeights[0] + 1.0f);
        const auto middle = visibleConsoleLogRows(layout, layout.rowOffsets[1], layout.rowHeights[1] + 1.0f);
        const auto bottom = visibleConsoleLogRows(layout, layout.rowOffsets[2], layout.rowHeights[2]);
        check(top.first == 0 && top.second == 1, "top visible-row range included a non-visible row");
        check(middle.first == 1 && middle.second == 2, "middle visible-row range lost its source row");
        check(bottom.first == 2 && bottom.second == 3, "bottom visible-row range lost its last row");
    } else {
        check(false, "visible-row range checks require three cached rows");
    }

    const size_t fullRebuildsBeforeWrap = layout.fullRebuilds;
    ensureConsoleLogLayout(layout, store, monoFont, fontSize, true, 80.0f, itemSpacingY);
    const float narrowWrappedHeight = layout.rowHeights.size() == 3 ? layout.rowHeights[2] : 0.0f;
    check(layout.wrap && layout.wrapWidth == 80.0f && layout.fullRebuilds == fullRebuildsBeforeWrap + 1,
          "wrap toggle did not rebuild variable-height layout");
    check(narrowWrappedHeight > fontSize, "long wrapped entry did not occupy multiple lines");

    ensureConsoleLogLayout(layout, store, monoFont, fontSize, true, 160.0f, itemSpacingY);
    const float wideWrappedHeight = layout.rowHeights.size() == 3 ? layout.rowHeights[2] : 0.0f;
    check(layout.wrapWidth == 160.0f && wideWrappedHeight < narrowWrappedHeight,
          "available-width change did not update wrapped row height");

    const size_t rebuildsBeforeFontChange = layout.fullRebuilds;
    ensureConsoleLogLayout(layout, store, monoFont, fontSize + 2.0f, true, 160.0f, itemSpacingY);
    check(layout.fullRebuilds == rebuildsBeforeFontChange + 1 && layout.rowHeights.size() == 3 &&
              layout.rowHeights[0] > fontSize,
          "font-size change did not invalidate and rebuild row heights");

    const size_t rebuildsBeforeAppend = layout.fullRebuilds;
    const size_t appendedRowsBeforeAppend = layout.appendedRowsMeasured;
    store.append({"appended\nentry", ImVec4(1, 1, 1, 1), ux3d::core::LogLevel::Fatal});
    ensureConsoleLogLayout(layout, store, monoFont, fontSize + 2.0f, true, 160.0f, itemSpacingY);
    check(layout.fullRebuilds == rebuildsBeforeAppend && layout.entryCount == 4 &&
              layout.appendedRowsMeasured == appendedRowsBeforeAppend + 1,
          "append failed to extend the cached layout without rebuilding existing rows");
    check(store.counts().errors == 2 &&
              buildConsoleLogCopyText(store.entries()).find("appended\nentry\n") != std::string::npos,
          "append changed severity totals or Copy contents");

    store.clear();
    ensureConsoleLogLayout(layout, store, monoFont, fontSize + 2.0f, true, 160.0f, itemSpacingY);
    const auto emptyRange = visibleConsoleLogRows(layout, 0.0f, 100.0f);
    check(store.size() == 0 && store.counts().errors == 0 && store.counts().warnings == 0 && store.counts().info == 0 &&
              buildConsoleLogCopyText(store.entries()).empty(),
          "Clear did not reset entries, severity totals and Copy contents");
    check(layout.rowOffsets.empty() && emptyRange.first == 0 && emptyRange.second == 0,
          "empty log layout produced visible rows");

    ConsoleLogStore fixedStore;
    fixedStore.append({"single fixed-height row", ImVec4(1, 1, 1, 1), ux3d::core::LogLevel::Info});
    ConsoleLogLayoutCache fixedLayout;
    ensureConsoleLogLayout(fixedLayout, fixedStore, monoFont, fontSize, false, 500.0f, itemSpacingY);
    check(fixedLayout.allSingleLine && fixedLayout.rowHeights.size() == 1,
          "single-line logger entries were not classified for fixed-height clipping");

    ImGui::PopFont();
    if (forceFailure) {
        check(false, "forced failure requested");
    }

    result.success = failures.empty();
    std::ostringstream report;
    report << "Logger layout self-check\n"
           << "Checks: " << checks << "\n"
           << "Result: " << (result.success ? "PASS" : "FAIL") << "\n";
    for (const auto& failure : failures) {
        report << "Failure: " << failure << "\n";
    }
    result.report = report.str();
    return result;
}

void Application::logMessage(const std::string& msg, ImVec4 color, ux3d::core::LogLevel level)
{
    s_consoleLogs.append({msg, color, level});
    std::fprintf(stderr, "%s\n", msg.c_str());
    std::fflush(stderr);
}

void Application::drawLogger()
{
    if (showLogger) {
        if (ImGui::Begin(WND_LOGGER, &showLogger)) {

            const bool loggerClearClicked = ImGui::Button(ICON_MDI_DELETE_OUTLINE " Clear");
            ux3d::imgui::UiProbe::item("log.clear", 0);
            if (loggerClearClicked) {
                s_consoleLogs.clear();
            }
            ImGui::SameLine();
            const bool loggerCopyClicked = ImGui::Button(ICON_MDI_CONTENT_COPY " Copy");
            ux3d::imgui::UiProbe::item("log.copy", 0);
            if (loggerCopyClicked) {
                const std::string allLogs = buildConsoleLogCopyText(s_consoleLogs.entries());
                ImGui::SetClipboardText(allLogs.c_str());
            }
            ImGui::SameLine();
            ImGui::Checkbox(ICON_MDI_WRAP " Wrap", &loggerWrapArg);
            ux3d::imgui::UiProbe::item("log.wrap", 0);
            ImGui::SameLine();
            ImGui::Checkbox("Auto-Scroll", &loggerAutoScrollArg);
            ux3d::imgui::UiProbe::item("log.autoscroll", 0);
            ImGui::SameLine();
            ImGui::PushFont(ux3d::imgui::getMonoFont());
            ImGui::TextDisabled("| Messages: %zu", s_consoleLogs.size());
            ImGui::PopFont();
            ImGui::Separator();

            ImGuiWindowFlags childFlags = 0;
            if (!loggerWrapArg) {
                childFlags |= ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysHorizontalScrollbar;
            }

            const auto& entries = s_consoleLogs.entries();
            ImFont* monoFont = ux3d::imgui::getMonoFont();
            ImGui::PushFont(monoFont);
            const float fontSize = ImGui::GetFontSize();
            const float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
            if (!loggerWrapArg) {
                const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
                ensureConsoleLogLayout(s_consoleLogLayout, s_consoleLogs, monoFont, fontSize, false, availableWidth,
                                       itemSpacingY);
                ImGui::SetNextWindowContentSize(ImVec2(s_consoleLogLayout.maxTextWidth, 0.0f));
            }

            ImGui::BeginChild("LoggerScrollArea", ImVec2(0, 0), false, childFlags);
            if (loggerWrapArg) {
                const float wrapWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
                ensureConsoleLogLayout(s_consoleLogLayout, s_consoleLogs, monoFont, fontSize, true, wrapWidth,
                                       itemSpacingY);
            }

            if (!loggerWrapArg && s_consoleLogLayout.allSingleLine) {
                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(entries.size()));
                while (clipper.Step()) {
                    for (int rowIndex = clipper.DisplayStart; rowIndex < clipper.DisplayEnd; ++rowIndex) {
                        const auto& entry = entries[static_cast<size_t>(rowIndex)];
                        ImGui::TextColored(entry.color, "%s", entry.text.c_str());
                        ux3d::imgui::UiProbe::item("log.row", static_cast<long long>(&entry - entries.data()));
                    }
                }
            } else {
                // The clipper assumes uniform heights; draw only the intersecting cached rows here.
                const ImVec2 contentPosition = ImGui::GetCursorScreenPos();
                const ImVec2 available = ImGui::GetContentRegionAvail();
                const auto visibleRows = visibleConsoleLogRows(s_consoleLogLayout, ImGui::GetScrollY(), available.y);
                const float contentWidth = loggerWrapArg ? 0.0f : s_consoleLogLayout.maxTextWidth;
                ImGui::Dummy(ImVec2(contentWidth, s_consoleLogLayout.contentHeight));

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const float wrapWidth = loggerWrapArg ? std::max(1.0f, available.x) : 0.0f;
                for (size_t rowIndex = visibleRows.first; rowIndex < visibleRows.second; ++rowIndex) {
                    const auto& entry = entries[rowIndex];
                    drawList->AddText(
                        monoFont, fontSize,
                        ImVec2(contentPosition.x, contentPosition.y + s_consoleLogLayout.rowOffsets[rowIndex]),
                        ImGui::ColorConvertFloat4ToU32(entry.color), entry.text.c_str(), nullptr, wrapWidth);
                    ux3d::imgui::UiProbe::rect(
                        "log.row", static_cast<long long>(rowIndex),
                        ImVec2(contentPosition.x, contentPosition.y + s_consoleLogLayout.rowOffsets[rowIndex]),
                        ImVec2(contentPosition.x + std::max(1.0f, available.x),
                               contentPosition.y + s_consoleLogLayout.rowEnds[rowIndex]));
                }
            }
            if (loggerAutoScrollArg && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
            ImGui::PopFont();
        }
        ImGui::End();
    }
}

void Application::ConsoleLogCallback::log(ux3d::core::LogLevel level, const char* message)
{

    if (!message) return;
    ImVec4 color(0.85f, 0.85f, 0.88f, 1.0f);
    if (level == ux3d::core::LogLevel::Warning) {
        color = ImVec4(1.0f, 0.8f, 0.2f, 1.0f);
    } else if (level == ux3d::core::LogLevel::Error || level == ux3d::core::LogLevel::Fatal) {
        color = ImVec4(1.0f, 0.35f, 0.35f, 1.0f);
    } else if (level == ux3d::core::LogLevel::Info) {
        color = ImVec4(0.5f, 0.85f, 1.0f, 1.0f);
    }
    owner.logMessage(message, color, level);
}

} // namespace ux3d::imgui::application
