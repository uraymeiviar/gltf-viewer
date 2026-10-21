// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#pragma once

#include "gltf/Gltf.h"
#include "imgui/ResourcesPanel.h" // For SelectionType
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <chrono>

namespace ux3d::imgui {

enum class ValidationSeverity {
    Error = 0,   // Red: glTF 2.0 specification violation
    Warning = 1, // Yellow: Performance issue, questionable data, deprecated feature
    Info = 2     // Blue: Hint, optimization recommendation, best practice
};

struct ValidationIssue {
    ValidationSeverity severity = ValidationSeverity::Info;
    std::string code;         // e.g. "SCHEMA_VIOLATION", "BUFFER_VIEW_OUT_OF_BOUNDS", "INDEX_OUT_OF_RANGE"
    std::string message;      // Human-readable description
    std::string jsonPointer;  // e.g. "/accessors/2/bufferView" or "/meshes/0/primitives/0"
    
    SelectionType targetType = SelectionType::None;
    int targetIndex = -1;     // e.g. node 3, mesh 0, material 1, animation 0
    void* targetObject = nullptr; // pointer to Node*, Material*, Mesh*, etc.

    const char* getSeverityString() const {
        switch (severity) {
            case ValidationSeverity::Error:   return "Error";
            case ValidationSeverity::Warning: return "Warning";
            case ValidationSeverity::Info:    return "Info";
            default:                          return "Unknown";
        }
    }
};

struct ValidationReport {
    std::string filePath;
    std::vector<ValidationIssue> issues;
    int errorCount = 0;
    int warningCount = 0;
    int infoCount = 0;
    double durationMs = 0.0;
    bool validated = false;

    void clear() {
        issues.clear();
        errorCount = 0;
        warningCount = 0;
        infoCount = 0;
        durationMs = 0.0;
        validated = false;
        filePath.clear();
    }

    void addIssue(ValidationSeverity sev,
                  const std::string& code,
                  const std::string& msg,
                  const std::string& ptr = "",
                  SelectionType type = SelectionType::None,
                  int index = -1,
                  void* obj = nullptr)
    {
        issues.push_back({sev, code, msg, ptr, type, index, obj});
        if (sev == ValidationSeverity::Error) errorCount++;
        else if (sev == ValidationSeverity::Warning) warningCount++;
        else if (sev == ValidationSeverity::Info) infoCount++;
    }

    std::string toMarkdown() const;
};

class GltfValidator {
public:
    static GltfValidator& instance();

    // Runs Tier 1 (Schema) and Tier 2 (Semantic & Buffer checks)
    ValidationReport validate(const std::string& filePath, ux3d::gltf::Gltf& gltfModel);

    // Get or clear last validation report
    const ValidationReport& getLastReport() const { return m_lastReport; }
    uint64_t getReportRevision() const { return m_reportRevision; }
    void clearReport()
    {
        m_lastReport.clear();
        ++m_reportRevision;
    }
    void setReport(const ValidationReport& report)
    {
        m_lastReport = report;
        ++m_reportRevision;
    }

private:
    GltfValidator();
    ~GltfValidator();

    GltfValidator(const GltfValidator&) = delete;
    GltfValidator& operator=(const GltfValidator&) = delete;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    ValidationReport m_lastReport;
    uint64_t m_reportRevision = 0;
};

} // namespace ux3d::imgui
