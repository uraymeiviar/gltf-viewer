// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/ValidationPanel.h"
#include "imgui/GltfValidator.h"
#include "imgui/IconsMaterialDesignIcons.h"
#include "imgui/SelfCheckResult.h"
#include "imgui/UiProbe.h"
#include "imgui/Theme.h"
#include "renderer/gltf/GltfRenderLayer.h"

#include <imgui.h>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace ux3d::imgui {

static bool s_filterErrors = true;
static bool s_filterWarnings = true;
static bool s_filterInfos = true;
static char s_searchBuffer[256] = "";
static int s_selectedIssueIndex = -1;
static bool s_focusRequested = false;
static int s_focusFrameCountdown = 0;

struct ValidationIssueSearchCache {
    std::string code;
    std::string message;
    std::string pointer;
};

struct ValidationCache {
    bool reportValid = false;
    uint64_t reportRevision = 0;
    std::vector<ValidationIssueSearchCache> issues;
    bool fixedHeightRows = true;

    bool filterValid = false;
    uint32_t severityMask = 0;
    std::string search;
    std::vector<int> visibleIssues;
};

static ValidationCache s_validationCache;

static std::string lowerSearchString(const std::string& text)
{
    std::string lower = text;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    return lower;
}

static uint32_t validationSeverityMask()
{
    return (s_filterErrors ? 1u : 0u)
        | (s_filterWarnings ? 2u : 0u)
        | (s_filterInfos ? 4u : 0u);
}

static void ensureValidationCache(const ValidationReport& report)
{
    const uint64_t reportRevision = GltfValidator::instance().getReportRevision();
    if (!s_validationCache.reportValid
        || s_validationCache.reportRevision != reportRevision
        || s_validationCache.issues.size() != report.issues.size()) {
        s_validationCache.reportValid = true;
        s_validationCache.reportRevision = reportRevision;
        s_validationCache.issues.resize(report.issues.size());
        s_validationCache.fixedHeightRows = true;
        for (size_t i = 0; i < report.issues.size(); ++i) {
            s_validationCache.issues[i].code = lowerSearchString(report.issues[i].code);
            s_validationCache.issues[i].message = lowerSearchString(report.issues[i].message);
            s_validationCache.issues[i].pointer = lowerSearchString(report.issues[i].jsonPointer);
            if (report.issues[i].code.find_first_of("\r\n") != std::string::npos
                || report.issues[i].message.find_first_of("\r\n") != std::string::npos
                || report.issues[i].jsonPointer.find_first_of("\r\n") != std::string::npos) {
                s_validationCache.fixedHeightRows = false;
            }
        }
        s_validationCache.filterValid = false;
    }

    const uint32_t severityMask = validationSeverityMask();
    const std::string search = lowerSearchString(s_searchBuffer);
    if (s_validationCache.filterValid
        && s_validationCache.severityMask == severityMask
        && s_validationCache.search == search) {
        return;
    }

    s_validationCache.filterValid = true;
    s_validationCache.severityMask = severityMask;
    s_validationCache.search = search;
    s_validationCache.visibleIssues.clear();
    s_validationCache.visibleIssues.reserve(report.issues.size());

    for (size_t i = 0; i < report.issues.size(); ++i) {
        const auto& issue = report.issues[i];
        const uint32_t severityBit = issue.severity == ValidationSeverity::Error ? 1u
            : issue.severity == ValidationSeverity::Warning ? 2u
            : 4u;
        if ((severityMask & severityBit) == 0) {
            continue;
        }

        if (!search.empty()) {
            const auto& searchCache = s_validationCache.issues[i];
            if (searchCache.code.find(search) == std::string::npos
                && searchCache.message.find(search) == std::string::npos
                && searchCache.pointer.find(search) == std::string::npos) {
                continue;
            }
        }

        s_validationCache.visibleIssues.push_back(static_cast<int>(i));
    }
}

void ValidationPanel::requestFocus()
{
    s_focusRequested = true;
    s_focusFrameCountdown = 4;
    std::string windowTitle = std::string(ICON_MDI_SHIELD_CHECK_OUTLINE) + " Validation###ValidationPanel";
    ImGui::SetWindowFocus(windowTitle.c_str());
}

void ValidationPanel::setSearchFilter(const std::string& search)
{
    std::snprintf(s_searchBuffer, sizeof(s_searchBuffer), "%s", search.c_str());
}

void ValidationPanel::setSeverityFilter(bool errors, bool warnings, bool infos)
{
    s_filterErrors = errors;
    s_filterWarnings = warnings;
    s_filterInfos = infos;
}

SelfCheckResult ValidationPanel::runCacheSelfCheck(bool forceFailure)
{
    constexpr size_t issueCount = 20000;
    ValidationReport report;
    report.validated = true;
    report.filePath = "synthetic-validation-report";
    for (size_t i = 0; i < issueCount; ++i) {
        const ValidationSeverity severity = (i % 3) == 0 ? ValidationSeverity::Error
            : (i % 3) == 1 ? ValidationSeverity::Warning
            : ValidationSeverity::Info;
        const std::string suffix = std::to_string(i);
        std::ostringstream codeStream;
        codeStream << "CODE_" << std::setw(5) << std::setfill('0') << i;
        const std::string code = codeStream.str();
        const std::string message = (i % 500) == 0
            ? "Contains Needle marker in synthetic issue " + suffix
            : "Synthetic issue " + suffix;
        report.addIssue(severity, code, message, "/nodes/" + suffix);
    }
    GltfValidator::instance().setReport(report);

    int checks = 0;
    std::vector<std::string> failures;
    auto check = [&](bool condition, const std::string& message) {
        ++checks;
        if (!condition && failures.size() < 20) {
            failures.push_back(message);
        }
    };

    auto legacyFilter = [&](bool errors, bool warnings, bool infos, const std::string& search) {
        const std::string lowerSearch = lowerSearchString(search);
        std::vector<int> visible;
        visible.reserve(report.issues.size());
        for (size_t i = 0; i < report.issues.size(); ++i) {
            const auto& issue = report.issues[i];
            if (issue.severity == ValidationSeverity::Error && !errors) continue;
            if (issue.severity == ValidationSeverity::Warning && !warnings) continue;
            if (issue.severity == ValidationSeverity::Info && !infos) continue;

            if (!lowerSearch.empty()) {
                if (lowerSearchString(issue.code).find(lowerSearch) == std::string::npos
                    && lowerSearchString(issue.message).find(lowerSearch) == std::string::npos
                    && lowerSearchString(issue.jsonPointer).find(lowerSearch) == std::string::npos) {
                    continue;
                }
            }
            visible.push_back(static_cast<int>(i));
        }
        return visible;
    };

    auto indexedFilter = [&](bool errors, bool warnings, bool infos, const std::string& search) {
        setSeverityFilter(errors, warnings, infos);
        setSearchFilter(search);
        s_validationCache.filterValid = false;
        ensureValidationCache(report);
        return s_validationCache.visibleIssues;
    };

    struct FilterCase {
        bool errors;
        bool warnings;
        bool infos;
        const char* search;
        size_t expected;
    };
    const FilterCase cases[] = {
        { true, true, true, "", issueCount },
        { true, false, false, "", 6667 },
        { false, true, false, "", 6667 },
        { false, false, true, "", 6666 },
        { true, true, true, "needle", 40 },
        { true, true, true, "code_01999", 1 },
        { true, true, true, "nodes/19990", 1 },
        { true, true, true, "no-such-issue", 0 },
    };

    for (const auto& filterCase : cases) {
        const auto legacy = legacyFilter(filterCase.errors, filterCase.warnings, filterCase.infos, filterCase.search);
        const auto indexed = indexedFilter(filterCase.errors, filterCase.warnings, filterCase.infos, filterCase.search);
        check(legacy == indexed, std::string("filter mismatch for search '") + filterCase.search + "'");
        check(legacy.size() == filterCase.expected,
            std::string("unexpected count for search '") + filterCase.search + "'");
        if (std::string(filterCase.search) == "code_01999") {
            check(indexed.size() == 1 && indexed[0] == 1999,
                "filtered validation row lost its original source issue index");
        } else if (std::string(filterCase.search) == "nodes/19990") {
            check(indexed.size() == 1 && indexed[0] == 19990,
                "pointer-filtered validation row lost its original source issue index");
        }
    }

    auto measureMs = [](int repetitions, const std::function<void()>& operation) {
        double bestMs = 1e100;
        for (int repetition = 0; repetition < repetitions; ++repetition) {
            const auto start = std::chrono::steady_clock::now();
            operation();
            const auto end = std::chrono::steady_clock::now();
            const double elapsed = std::chrono::duration<double, std::milli>(end - start).count();
            bestMs = std::min(bestMs, elapsed);
        }
        return bestMs;
    };

    const std::string benchmarkSearch = "needle";
    s_validationCache = ValidationCache{};
    setSeverityFilter(true, true, true);
    setSearchFilter(benchmarkSearch);
    const auto coldStart = std::chrono::steady_clock::now();
    ensureValidationCache(report);
    const auto coldEnd = std::chrono::steady_clock::now();
    const double coldMs = std::chrono::duration<double, std::milli>(coldEnd - coldStart).count();
    check(s_validationCache.visibleIssues.size() == 40, "cold cache visible count");

    const double legacyMs = measureMs(5, [&]() {
        (void)legacyFilter(true, true, true, benchmarkSearch);
    });
    const double indexedWarmMs = measureMs(50, [&]() {
        s_validationCache.filterValid = false;
        ensureValidationCache(report);
    });
    check(s_validationCache.visibleIssues.size() == 40, "warm cache visible count");

    if (forceFailure) {
        check(false, "forced validation cache self-check failure");
    }

    std::ostringstream output;
    output << "P2.2 validation filter cache self-check\n";
    output << "Result: " << (failures.empty() ? "PASS" : "FAIL") << "\n";
    output << "Checks: " << checks << "\n";
    output << "Issues: " << issueCount << " (errors " << report.errorCount
           << ", warnings " << report.warningCount << ", infos " << report.infoCount << ")\n";
    output << "Coverage: empty search, all severities, errors-only, warnings-only, infos-only, "
              "case-insensitive code/message/pointer search, no-match search\n";
    output << std::fixed << std::setprecision(3);
    output << "Legacy per-frame filter: " << legacyMs << " ms; indexed warm filter: " << indexedWarmMs
           << " ms; speedup: " << (indexedWarmMs > 0.0 ? legacyMs / indexedWarmMs : 0.0) << "x\n";
    output << "Indexed cold build+filter: " << coldMs << " ms\n";
    for (const auto& failure : failures) {
        output << "FAIL: " << failure << "\n";
    }
    return SelfCheckResult{ failures.empty(), output.str() };
}

SelfCheckResult ValidationPanel::runReportRevisionSelfCheck(bool forceFailure)
{
    int checks = 0;
    std::vector<std::string> failures;
    auto check = [&](bool condition, const std::string& message) {
        ++checks;
        if (!condition && failures.size() < 20) {
            failures.push_back(message);
        }
    };

    ValidationReport reportA;
    reportA.validated = true;
    reportA.filePath = "synthetic-report-a";
    reportA.addIssue(ValidationSeverity::Error, "ALPHA_ERROR_1", "alpha error one", "/alpha/0");
    reportA.addIssue(ValidationSeverity::Error, "ALPHA_ERROR_2", "alpha error two", "/alpha/1");
    reportA.addIssue(ValidationSeverity::Warning, "ALPHA_WARNING_1", "alpha warning one", "/alpha/2");
    reportA.addIssue(ValidationSeverity::Warning, "ALPHA_WARNING_2", "alpha warning two", "/alpha/3");

    ValidationReport reportB;
    reportB.validated = true;
    reportB.filePath = "synthetic-report-b";
    reportB.addIssue(ValidationSeverity::Error, "BETA_ERROR_1", "beta error one", "/beta/0");
    reportB.addIssue(ValidationSeverity::Error, "BETA_ERROR_2", "beta error two", "/beta/1");
    reportB.addIssue(ValidationSeverity::Info, "BETA_INFO_1", "beta info one", "/beta/2");
    reportB.addIssue(ValidationSeverity::Info, "BETA_INFO_2", "beta info two", "/beta/3");

    check(reportA.issues.size() == reportB.issues.size(), "replacement reports do not have equal issue counts");
    check(reportA.errorCount == 2 && reportA.warningCount == 2 && reportA.infoCount == 0,
        "report A severity counts mismatch");
    check(reportB.errorCount == 2 && reportB.warningCount == 0 && reportB.infoCount == 2,
        "report B severity counts mismatch");

    auto& validator = GltfValidator::instance();
    validator.setReport(reportA);
    const uint64_t revisionA = validator.getReportRevision();
    s_validationCache = ValidationCache{};

    setSeverityFilter(true, false, false);
    setSearchFilter("alpha");
    ensureValidationCache(validator.getLastReport());
    check(s_validationCache.visibleIssues.size() == 2,
        "report A errors + alpha filter count mismatch");
    if (s_validationCache.visibleIssues.size() == 2) {
        check(s_validationCache.visibleIssues[0] == 0 && s_validationCache.visibleIssues[1] == 1,
            "report A visible row identity mismatch");
    }

    setSeverityFilter(false, true, false);
    ensureValidationCache(validator.getLastReport());
    check(s_validationCache.visibleIssues.size() == 2,
        "report A warnings + alpha filter count mismatch");

    // Restore errors-only + alpha, then replace the report without touching
    // either control. Only the report revision/content changes here.
    setSeverityFilter(true, false, false);
    setSearchFilter("alpha");
    ensureValidationCache(validator.getLastReport());

    validator.setReport(reportB);
    const uint64_t revisionB = validator.getReportRevision();
    check(revisionA != revisionB, "report replacement did not bump the report revision");

    setSeverityFilter(true, false, false);
    setSearchFilter("alpha");
    ensureValidationCache(validator.getLastReport());
    check(s_validationCache.visibleIssues.empty(),
        "equal-count report replacement reused stale alpha rows");

    setSearchFilter("beta");
    ensureValidationCache(validator.getLastReport());
    check(s_validationCache.visibleIssues.size() == 2,
        "report B errors + beta filter count mismatch");
    if (s_validationCache.visibleIssues.size() == 2) {
        check(s_validationCache.visibleIssues[0] == 0 && s_validationCache.visibleIssues[1] == 1,
            "report B visible row identity mismatch");
    }

    setSeverityFilter(false, true, false);
    ensureValidationCache(validator.getLastReport());
    check(s_validationCache.visibleIssues.empty(),
        "report B warnings filter retained report A severities");

    setSeverityFilter(false, false, true);
    ensureValidationCache(validator.getLastReport());
    check(s_validationCache.visibleIssues.size() == 2,
        "report B infos filter count mismatch");

    ValidationReport multilineReport;
    multilineReport.validated = true;
    multilineReport.addIssue(ValidationSeverity::Error, "MULTILINE_MESSAGE",
        "first message line\nsecond message line", "/multiline/0");
    validator.setReport(multilineReport);
    setSeverityFilter(true, true, true);
    setSearchFilter("");
    ensureValidationCache(validator.getLastReport());
    check(!s_validationCache.fixedHeightRows && s_validationCache.visibleIssues.size() == 1,
        "multiline validation row was incorrectly classified as fixed-height");

    if (forceFailure) {
        check(false, "forced report-revision self-check failure");
    }

    std::ostringstream output;
    output << "P2.2 validation report-revision self-check\n";
    output << "Result: " << (failures.empty() ? "PASS" : "FAIL") << "\n";
    output << "Checks: " << checks << "\n";
    output << "Reports: equal issue count (" << reportA.issues.size() << "), different codes, pointers and severities\n";
    output << "Coverage: cached filter/search reuse across report replacement, severity changes, "
              "report revision invalidation independent of issue count\n";
    for (const auto& failure : failures) {
        output << "FAIL: " << failure << "\n";
    }

    setSeverityFilter(true, true, true);
    setSearchFilter("");
    return SelfCheckResult{ failures.empty(), output.str() };
}

void ValidationPanel::render(bool* pOpen,
                             const std::string& currentFilePath,
                             ux3d::gltf::Gltf& gltfModel,
                             ux3d::imgui::SelectionState& selection,
                             ux3d::editor::CameraActor& cameraActor,
                             ux3d::renderer::GltfRenderLayer* gltfLayer)
{
    if (pOpen != nullptr && !*pOpen) return;

    ImGui::SetNextWindowSize(ImVec2(750, 420), ImGuiCond_FirstUseEver);

    if (s_focusRequested || s_focusFrameCountdown > 0) {
        ImGui::SetNextWindowFocus();
        if (s_focusFrameCountdown > 0) s_focusFrameCountdown--;
        s_focusRequested = false;
    }

    std::string windowTitle = std::string(ICON_MDI_SHIELD_CHECK_OUTLINE) + " Validation###ValidationPanel";
    if (!ImGui::Begin(windowTitle.c_str(), pOpen)) {
        ImGui::End();
        return;
    }

    auto& validator = GltfValidator::instance();
    const auto& report = validator.getLastReport();

    // -----------------------------------------------------------------
    // Header & Summary Status Bar
    // -----------------------------------------------------------------
    if (!report.validated) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s Validation has not run on current model.", ICON_MDI_INFORMATION_OUTLINE);
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_REFRESH " Run Validation")) {
            validator.validate(currentFilePath, gltfModel);
        }
    } else {
        if (report.errorCount > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                               "%s Validation Failed: %d %s, %d %s (Scan: %.1f ms)",
                               ICON_MDI_ALERT_OCTAGON,
                               report.errorCount, report.errorCount == 1 ? "Error" : "Errors",
                               report.warningCount, report.warningCount == 1 ? "Warning" : "Warnings",
                               report.durationMs);
        } else if (report.warningCount > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.25f, 1.0f),
                               "%s Valid glTF with %d %s (Scan: %.1f ms)",
                               ICON_MDI_ALERT,
                               report.warningCount, report.warningCount == 1 ? "Warning" : "Warnings",
                               report.durationMs);
        } else {
            ImGui::TextColored(ImVec4(0.2f, 0.95f, 0.5f, 1.0f),
                               "%s Valid glTF 2.0 Asset (0 Errors, 0 Warnings, Scan: %.1f ms)",
                               ICON_MDI_CHECK_CIRCLE,
                               report.durationMs);
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 250);
        if (ImGui::Button(ICON_MDI_REFRESH " Re-validate")) {
            validator.validate(currentFilePath, gltfModel);
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MDI_CONTENT_COPY " Copy Report")) {
            ImGui::SetClipboardText(report.toMarkdown().c_str());
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Copy full validation report as Markdown to clipboard.");
        }
    }

    ImGui::Separator();

    // -----------------------------------------------------------------
    // Filter & Search Controls
    // -----------------------------------------------------------------
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Filter:");
    ImGui::SameLine();

    // Errors filter toggle
    char errLabel[64];
    std::snprintf(errLabel, sizeof(errLabel), "%s Errors (%d)###FiltErr", ICON_MDI_ALERT_OCTAGON, report.errorCount);
    if (s_filterErrors) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
    }
    if (ImGui::Button(errLabel)) {
        s_filterErrors = !s_filterErrors;
    }
    if (s_filterErrors) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Warnings filter toggle
    char warnLabel[64];
    std::snprintf(warnLabel, sizeof(warnLabel), "%s Warnings (%d)###FiltWarn", ICON_MDI_ALERT, report.warningCount);
    if (s_filterWarnings) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.45f, 0.1f, 1.0f));
    }
    if (ImGui::Button(warnLabel)) {
        s_filterWarnings = !s_filterWarnings;
    }
    if (s_filterWarnings) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();

    // Infos filter toggle
    char infoLabel[64];
    std::snprintf(infoLabel, sizeof(infoLabel), "%s Infos (%d)###FiltInfo", ICON_MDI_INFORMATION, report.infoCount);
    if (s_filterInfos) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.6f, 1.0f));
    }
    if (ImGui::Button(infoLabel)) {
        s_filterInfos = !s_filterInfos;
    }
    if (s_filterInfos) {
        ImGui::PopStyleColor();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##IssueSearch", "Search issues...", s_searchBuffer, sizeof(s_searchBuffer));
    ux3d::imgui::UiProbe::item("val.search", 0);
    if (s_searchBuffer[0] != '\0') {
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MDI_CLOSE)) {
            s_searchBuffer[0] = '\0';
        }
    }

    ImGui::Spacing();

    // -----------------------------------------------------------------
    // Issues Table
    // -----------------------------------------------------------------
    static ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable |
                                       ImGuiTableFlags_Reorderable |
                                       ImGuiTableFlags_RowBg |
                                       ImGuiTableFlags_Borders |
                                       ImGuiTableFlags_ScrollY;

    if (ImGui::BeginTable("IssuesTable", 4, tableFlags, ImVec2(0, 0))) {
        ImGui::TableSetupColumn("Severity", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("Code", ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn("Pointer / Target", ImGuiTableColumnFlags_WidthFixed, 160.0f);
        ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        ensureValidationCache(report);

        auto renderIssueRow = [&](int rowIdx) {
            const auto& issue = report.issues[static_cast<size_t>(rowIdx)];

            ImGui::TableNextRow();

            // Column 1: Severity
            ImGui::TableSetColumnIndex(0);
            ImVec4 sevColor = (issue.severity == ValidationSeverity::Error) ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f) :
                              (issue.severity == ValidationSeverity::Warning) ? ImVec4(1.0f, 0.85f, 0.25f, 1.0f) :
                              ImVec4(0.4f, 0.75f, 1.0f, 1.0f);
            const char* sevIcon = (issue.severity == ValidationSeverity::Error) ? ICON_MDI_ALERT_OCTAGON :
                                  (issue.severity == ValidationSeverity::Warning) ? ICON_MDI_ALERT :
                                  ICON_MDI_INFORMATION;

            bool isSelected = (s_selectedIssueIndex == rowIdx);
            char rowSelectableId[64];
            std::snprintf(rowSelectableId, sizeof(rowSelectableId), "##IssueRow_%d", rowIdx);

            const bool rowClicked = ImGui::Selectable(rowSelectableId, isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick);
            ux3d::imgui::UiProbe::item("val.row", rowIdx);
            if (rowClicked) {
                s_selectedIssueIndex = rowIdx;

                // Select target object in application inspector
                if (issue.targetObject != nullptr) {
                    switch (issue.targetType) {
                        case SelectionType::Node:
                            selection.selectNode(static_cast<ux3d::gltf::Node*>(issue.targetObject));
                            break;
                        case SelectionType::Material:
                            selection.selectMaterial(static_cast<ux3d::gltf::Material*>(issue.targetObject));
                            break;
                        case SelectionType::Mesh:
                            selection.selectMesh(static_cast<ux3d::gltf::Mesh*>(issue.targetObject));
                            break;
                        case SelectionType::Camera:
                            selection.selectCamera(static_cast<ux3d::gltf::Camera*>(issue.targetObject));
                            break;
                        case SelectionType::Skin:
                            selection.selectSkin(static_cast<ux3d::gltf::Skin*>(issue.targetObject));
                            break;
                        case SelectionType::Animation:
                            selection.selectAnimation(static_cast<ux3d::gltf::Animation*>(issue.targetObject));
                            break;
                        case SelectionType::Texture:
                            selection.selectTexture(static_cast<ux3d::gltf::Texture*>(issue.targetObject));
                            break;
                        case SelectionType::Image:
                            selection.selectImage(static_cast<ux3d::gltf::Image*>(issue.targetObject));
                            break;
                        default: break;
                    }
                }

                // If double-clicked on a row with a node, focus camera on it
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (issue.targetType == SelectionType::Node && selection.selectedNode != nullptr) {
                        cameraActor.presentFocusNode(gltfLayer, selection.selectedNode, &gltfModel);
                    }
                }
            }

            ImGui::SameLine();
            ImGui::TextColored(sevColor, "%s %s", sevIcon, issue.getSeverityString());

            // Column 2: Code
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(issue.code.c_str());

            // Column 3: Location / JSON Pointer
            ImGui::TableSetColumnIndex(2);
            if (!issue.jsonPointer.empty()) {
                ImGui::TextUnformatted(issue.jsonPointer.c_str());
            } else if (issue.targetIndex >= 0) {
                ImGui::Text("[%d]", issue.targetIndex);
            } else {
                ImGui::TextDisabled("-");
            }

            // Column 4: Message
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(issue.message.c_str());
            ux3d::imgui::UiProbe::item("val.msg", rowIdx);

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ux3d::imgui::UiProbe::mark("val.tooltip", rowIdx);
                ImGui::TextColored(sevColor, "%s %s: %s", sevIcon, issue.getSeverityString(), issue.code.c_str());
                if (!issue.jsonPointer.empty()) {
                    ImGui::TextDisabled("Location: %s", issue.jsonPointer.c_str());
                }
                ImGui::Separator();
                ImGui::TextWrapped("%s", issue.message.c_str());
                if (issue.targetObject != nullptr) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Double-click to focus and inspect object.");
                }
                ImGui::EndTooltip();
            }
        };

        if (s_validationCache.fixedHeightRows) {
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(s_validationCache.visibleIssues.size()));
            while (clipper.Step()) {
                for (int visibleIndex = clipper.DisplayStart; visibleIndex < clipper.DisplayEnd; ++visibleIndex) {
                    renderIssueRow(s_validationCache.visibleIssues[static_cast<size_t>(visibleIndex)]);
                }
            }
        } else {
            for (int rowIdx : s_validationCache.visibleIssues) {
                renderIssueRow(rowIdx);
            }
        }

        ux3d::imgui::UiProbe::mark("val.selected", s_selectedIssueIndex);
        ImGui::EndTable();
    }

    ImGui::End();
}

void ValidationPanel::renderStatusBarIndicator(bool* pOpenValidationPanel)
{
    const auto& report = GltfValidator::instance().getLastReport();

    if (!report.validated) {
        if (ImGui::SmallButton(ICON_MDI_SHIELD_OUTLINE " Validate")) {
            if (pOpenValidationPanel) *pOpenValidationPanel = true;
            requestFocus();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Run glTF 2.0 specification validation.");
        }
        return;
    }

    if (report.errorCount > 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %d %s", ICON_MDI_ALERT_OCTAGON, report.errorCount, report.errorCount == 1 ? "Error" : "Errors");
        if (ImGui::SmallButton(buf)) {
            if (pOpenValidationPanel) *pOpenValidationPanel = true;
            requestFocus();
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Asset has %d validation %s and %d %s.\nClick to inspect in Validation panel.",
                              report.errorCount, report.errorCount == 1 ? "error" : "errors",
                              report.warningCount, report.warningCount == 1 ? "warning" : "warnings");
        }
    } else if (report.warningCount > 0) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.45f, 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.58f, 0.15f, 1.0f));
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %d %s", ICON_MDI_ALERT, report.warningCount, report.warningCount == 1 ? "Warning" : "Warnings");
        if (ImGui::SmallButton(buf)) {
            if (pOpenValidationPanel) *pOpenValidationPanel = true;
            requestFocus();
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Asset is valid with %d %s.\nClick to view in Validation panel.",
                              report.warningCount, report.warningCount == 1 ? "warning" : "warnings");
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.45f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.6f, 0.36f, 1.0f));
        if (ImGui::SmallButton(ICON_MDI_CHECK_CIRCLE " glTF Valid")) {
            if (pOpenValidationPanel) *pOpenValidationPanel = true;
            requestFocus();
        }
        ImGui::PopStyleColor(2);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Asset strictly complies with glTF 2.0 specification.\nClick to view report.");
        }
    }
}

} // namespace ux3d::imgui
