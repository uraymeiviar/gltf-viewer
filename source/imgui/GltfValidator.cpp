// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "imgui/GltfValidator.h"
#include "imgui/EmbeddedVFS.h"
#include "imgui/StdFileSystem.h"

#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/Node.h"
#include "gltf/Mesh.h"
#include "gltf/Material.h"
#include "gltf/Accessor.h"
#include "gltf/BufferView.h"
#include "gltf/Buffer.h"
#include "gltf/Animation.h"
#include "gltf/Camera.h"
#include "gltf/Skin.h"
#include "gltf/Scene.h"
#include "gltf/Texture.h"
#include "gltf/Image.h"
#include "gltf/Sampler.h"
#include "gltf/GlTypes.h"

#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>

#include <cmath>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cstring>

namespace ux3d::imgui {

// Custom schema error collector
class ValidatorErrorHandler : public nlohmann::json_schema::error_handler {
public:
    explicit ValidatorErrorHandler(ValidationReport& report)
        : m_report(report)
    {
    }

    void error(const nlohmann::json::json_pointer& ptr, const nlohmann::json& instance, const std::string& message) override {
        std::string pstr = ptr.to_string();
        SelectionType type = SelectionType::None;
        int idx = -1;

        if (pstr.rfind("/nodes/", 0) == 0) {
            type = SelectionType::Node;
            std::sscanf(pstr.c_str(), "/nodes/%d", &idx);
        } else if (pstr.rfind("/materials/", 0) == 0) {
            type = SelectionType::Material;
            std::sscanf(pstr.c_str(), "/materials/%d", &idx);
        } else if (pstr.rfind("/meshes/", 0) == 0) {
            type = SelectionType::Mesh;
            std::sscanf(pstr.c_str(), "/meshes/%d", &idx);
        } else if (pstr.rfind("/animations/", 0) == 0) {
            type = SelectionType::Animation;
            std::sscanf(pstr.c_str(), "/animations/%d", &idx);
        } else if (pstr.rfind("/cameras/", 0) == 0) {
            type = SelectionType::Camera;
            std::sscanf(pstr.c_str(), "/cameras/%d", &idx);
        } else if (pstr.rfind("/textures/", 0) == 0) {
            type = SelectionType::Texture;
            std::sscanf(pstr.c_str(), "/textures/%d", &idx);
        } else if (pstr.rfind("/images/", 0) == 0) {
            type = SelectionType::Image;
            std::sscanf(pstr.c_str(), "/images/%d", &idx);
        } else if (pstr.rfind("/skins/", 0) == 0) {
            type = SelectionType::Skin;
            std::sscanf(pstr.c_str(), "/skins/%d", &idx);
        }

        m_report.addIssue(ValidationSeverity::Error, "SCHEMA_VALIDATION_ERROR", message, pstr, type, idx);
    }

private:
    ValidationReport& m_report;
};

struct GltfValidator::Impl {
    std::unique_ptr<nlohmann::json_schema::json_validator> m_validator;
    bool m_initialized = false;

    void ensureInitialized() {
        if (m_initialized) return;

        auto schema_loader = [](const nlohmann::json_uri& id, nlohmann::json& schemaValue) {
            std::string path = id.path();
            if (!path.empty() && path[0] == '/') path = path.substr(1);
            size_t slash = path.rfind('/');
            std::string fn = (slash != std::string::npos) ? path.substr(slash + 1) : path;

            auto& vfs = EmbeddedVFS::instance();
            VfsFile vf;
            const std::string candidates[] = {
                path,
                "schemas/gltf/" + fn,
                "schemas/gltf/" + path,
                "schemas/extensions/" + fn,
                "schemas/extensions/" + path,
                "schemas/" + fn,
                "schemas/" + path
            };

            for (const auto& c : candidates) {
                vf = vfs.getFile(c);
                if (vf.isValid()) break;
            }

            if (vf.isValid()) {
                schemaValue = nlohmann::json::parse(vf.asStringView());
            } else {
                throw std::runtime_error("Referenced glTF schema not found in EmbeddedVFS: " + path);
            }
        };

        auto format_checker = [](const std::string& format, const std::string& value) {
            if (format == "date-time" || format == "email" || format == "ipv4" || format == "ipv6" || format == "uri") {
                nlohmann::json_schema::default_string_format_check(format, value);
            }
        };

        m_validator = std::make_unique<nlohmann::json_schema::json_validator>(schema_loader, format_checker);

        auto& vfs = EmbeddedVFS::instance();
        VfsFile rootSchemaFile = vfs.getFile("schemas/gltf/glTF.schema.json");
        if (!rootSchemaFile.isValid()) {
            rootSchemaFile = vfs.getFile("glTF.schema.json");
        }

        if (rootSchemaFile.isValid()) {
            try {
                auto rootSchema = nlohmann::json::parse(rootSchemaFile.asStringView());
                m_validator->set_root_schema(rootSchema);
                m_initialized = true;
            } catch (const std::exception& e) {
                m_initialized = false;
            }
        }
    }
};

GltfValidator::GltfValidator()
    : m_impl(std::make_unique<Impl>())
{
}

GltfValidator::~GltfValidator() = default;

GltfValidator& GltfValidator::instance()
{
    static GltfValidator s_instance;
    return s_instance;
}

std::string ValidationReport::toMarkdown() const
{
    std::ostringstream ss;
    ss << "# glTF 2.0 Validation Report\n\n";
    ss << "- **File**: `" << filePath << "`\n";
    ss << "- **Result**: " << (errorCount == 0 ? (warningCount == 0 ? "PASSED (Valid glTF)" : "PASSED WITH WARNINGS") : "FAILED (Invalid glTF)") << "\n";
    ss << "- **Errors**: " << errorCount << " | **Warnings**: " << warningCount << " | **Infos**: " << infoCount << "\n";
    ss << "- **Scan Duration**: " << durationMs << " ms\n\n";

    if (issues.empty()) {
        ss << "No issues detected. Asset conforms to glTF 2.0 specification.\n";
        return ss.str();
    }

    ss << "## Issues (" << issues.size() << ")\n\n";
    ss << "| Severity | Issue Code | Location / Pointer | Message |\n";
    ss << "| :--- | :--- | :--- | :--- |\n";

    for (const auto& issue : issues) {
        const char* sevBadge = (issue.severity == ValidationSeverity::Error) ? "🔴 Error" :
                               (issue.severity == ValidationSeverity::Warning) ? "🟡 Warning" : "🔵 Info";
        ss << "| " << sevBadge << " | `" << issue.code << "` | `"
           << (issue.jsonPointer.empty() ? "-" : issue.jsonPointer) << "` | "
           << issue.message << " |\n";
    }

    return ss.str();
}

ValidationReport GltfValidator::validate(const std::string& filePath, ux3d::gltf::Gltf& gltfModel)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    ValidationReport report;
    report.filePath = filePath;

    if (filePath.empty()) {
        report.addIssue(ValidationSeverity::Error, "EMPTY_FILE_PATH", "No file specified for validation.", "/");
        report.validated = true;
        return report;
    }

    // Index lookup for fast C++ object to index queries
    std::vector<ux3d::gltf::Node*> nodeList;
    for (auto& n : gltfModel.getElements<ux3d::gltf::Node>()) nodeList.push_back(&n);
    std::vector<ux3d::gltf::Material*> matList;
    for (auto& m : gltfModel.getElements<ux3d::gltf::Material>()) matList.push_back(&m);
    std::vector<ux3d::gltf::Mesh*> meshList;
    for (auto& m : gltfModel.getElements<ux3d::gltf::Mesh>()) meshList.push_back(&m);
    std::vector<ux3d::gltf::Animation*> animList;
    for (auto& a : gltfModel.getElements<ux3d::gltf::Animation>()) animList.push_back(&a);
    std::vector<ux3d::gltf::Camera*> camList;
    for (auto& c : gltfModel.getElements<ux3d::gltf::Camera>()) camList.push_back(&c);
    std::vector<ux3d::gltf::Skin*> skinList;
    for (auto& s : gltfModel.getElements<ux3d::gltf::Skin>()) skinList.push_back(&s);
    std::vector<ux3d::gltf::Texture*> texList;
    for (auto& t : gltfModel.getElements<ux3d::gltf::Texture>()) texList.push_back(&t);
    std::vector<ux3d::gltf::Image*> imgList;
    for (auto& img : gltfModel.getElements<ux3d::gltf::Image>()) imgList.push_back(&img);

    // -------------------------------------------------------------
    // Step 1: Ingest Raw Asset Text / Binary
    // -------------------------------------------------------------
    nlohmann::json jsonDoc;
    bool isGlb = (filePath.size() >= 4 && (filePath.compare(filePath.size() - 4, 4, ".glb") == 0 ||
                                          filePath.compare(filePath.size() - 4, 4, ".GLB") == 0));

    if (isGlb) {
        core::Vector<uint8_t> binaryData;
        auto fsFile = StdFileSystem::instance().toFile(filePath.c_str());
        if (core::Failed(fsFile.load(binaryData))) {
            report.addIssue(ValidationSeverity::Error, "FILE_READ_ERROR", "Could not read GLB file from filesystem: " + filePath, "/");
            report.validated = true;
            return report;
        }

        const uint8_t* bytes = binaryData.data();
        size_t totalSize = binaryData.size();

        // 1.1 Validate 12-byte GLB Header
        if (totalSize < 12) {
            report.addIssue(ValidationSeverity::Error, "GLB_HEADER_TOO_SMALL", "File length is under 12 bytes; invalid GLB container.", "/");
            report.validated = true;
            return report;
        }

        if (std::memcmp(bytes, "glTF", 4) != 0) {
            report.addIssue(ValidationSeverity::Error, "GLB_INVALID_MAGIC", "Container magic header is not 'glTF'.", "/");
            report.validated = true;
            return report;
        }

        uint32_t glbVersion = 0;
        uint32_t glbDeclaredLength = 0;
        std::memcpy(&glbVersion, bytes + 4, 4);
        std::memcpy(&glbDeclaredLength, bytes + 8, 4);

        if (glbVersion != 2) {
            report.addIssue(ValidationSeverity::Error, "GLB_UNSUPPORTED_VERSION", "GLB version is " + std::to_string(glbVersion) + ", only version 2 is supported.", "/");
        }

        if (glbDeclaredLength != totalSize) {
            report.addIssue(ValidationSeverity::Error, "GLB_LENGTH_MISMATCH",
                            "Declared GLB length (" + std::to_string(glbDeclaredLength) + " bytes) does not match actual file size (" + std::to_string(totalSize) + " bytes).", "/");
        }

        // 1.2 Validate Chunk 0 (JSON)
        if (totalSize < 20) {
            report.addIssue(ValidationSeverity::Error, "GLB_NO_JSON_CHUNK", "GLB missing initial JSON chunk header.", "/");
            report.validated = true;
            return report;
        }

        uint32_t chunk0Length = 0;
        uint32_t chunk0Type = 0;
        std::memcpy(&chunk0Length, bytes + 12, 4);
        std::memcpy(&chunk0Type, bytes + 16, 4);

        // "JSON" in ASCII is 0x4E4F534A
        if (chunk0Type != 0x4E4F534A) {
            report.addIssue(ValidationSeverity::Error, "GLB_INVALID_CHUNK_TYPE", "First GLB chunk type is not JSON (0x4E4F534A).", "/");
            report.validated = true;
            return report;
        }

        if (20 + chunk0Length > totalSize) {
            report.addIssue(ValidationSeverity::Error, "GLB_CHUNK_EXCEEDS_FILE", "JSON chunk extends past end of file.", "/");
            report.validated = true;
            return report;
        }

        std::string jsonStr(reinterpret_cast<const char*>(bytes + 20), chunk0Length);
        try {
            jsonDoc = nlohmann::json::parse(jsonStr);
        } catch (const std::exception& e) {
            report.addIssue(ValidationSeverity::Error, "JSON_SYNTAX_ERROR", std::string("Malformed JSON in GLB: ") + e.what(), "/");
            report.validated = true;
            return report;
        }

        // 1.3 Validate Chunk 1 (BIN) if present
        size_t chunk0Padding = (4 - (chunk0Length % 4)) % 4;
        size_t chunk1Offset = 20 + chunk0Length + chunk0Padding;
        if (chunk1Offset + 8 <= totalSize) {
            uint32_t chunk1Length = 0;
            uint32_t chunk1Type = 0;
            std::memcpy(&chunk1Length, bytes + chunk1Offset, 4);
            std::memcpy(&chunk1Type, bytes + chunk1Offset + 4, 4);

            // "BIN\0" is 0x004E4942
            if (chunk1Type != 0x004E4942) {
                report.addIssue(ValidationSeverity::Warning, "GLB_UNEXPECTED_CHUNK_TYPE", "Second GLB chunk is not BIN chunk.", "/");
            }
        }
    } else {
        // Plain text .gltf
        core::string textContent;
        auto fsFile = StdFileSystem::instance().toFile(filePath.c_str());
        if (core::Failed(fsFile.load(textContent))) {
            report.addIssue(ValidationSeverity::Error, "FILE_READ_ERROR", "Could not read glTF text file from filesystem: " + filePath, "/");
            report.validated = true;
            return report;
        }

        try {
            jsonDoc = nlohmann::json::parse(textContent.c_str());
        } catch (const std::exception& e) {
            report.addIssue(ValidationSeverity::Error, "JSON_SYNTAX_ERROR", std::string("Malformed glTF JSON syntax: ") + e.what(), "/");
            report.validated = true;
            return report;
        }
    }

    // -------------------------------------------------------------
    // Step 2: Tier 1 - In-Memory JSON Schema Validation
    // -------------------------------------------------------------
    m_impl->ensureInitialized();
    if (m_impl->m_initialized && m_impl->m_validator) {
        ValidatorErrorHandler errorHandler(report);
        try {
            m_impl->m_validator->validate(jsonDoc, errorHandler);
        } catch (const std::exception& e) {
            report.addIssue(ValidationSeverity::Error, "SCHEMA_VALIDATION_EXCEPTION", e.what(), "/");
        }
    }

    // -------------------------------------------------------------
    // Step 3: Tier 2 - Native C++ Semantic & Buffer Integrity Checks
    // -------------------------------------------------------------

    // 3.1 Asset Information
    if (jsonDoc.contains("asset") && jsonDoc["asset"].is_object()) {
        std::string version = jsonDoc["asset"].value("version", "");
        if (version != "2.0") {
            report.addIssue(ValidationSeverity::Error, "UNSUPPORTED_GLTF_VERSION",
                            "glTF asset.version is '" + version + "', only '2.0' is supported.", "/asset/version");
        }
    } else {
        report.addIssue(ValidationSeverity::Error, "MISSING_ASSET_OBJECT", "Root glTF missing mandatory 'asset' object.", "/asset");
    }

    // 3.2 BufferViews & Buffers
    auto& bufferViews = gltfModel.getElements<ux3d::gltf::BufferView>();
    int bvIdx = 0;
    for (const auto& bv : bufferViews) {
        std::string bvPtr = "/bufferViews/" + std::to_string(bvIdx);
        const auto* buf = bv.getBuffer();
        if (buf == nullptr) {
            report.addIssue(ValidationSeverity::Error, "BUFFER_VIEW_NULL_BUFFER", "BufferView does not reference a valid buffer.", bvPtr);
        } else {
            uint32_t offset = bv.hasByteOffset() ? bv.getByteOffset() : 0;
            uint32_t length = bv.getByteLength();
            uint32_t bufLen = buf->getByteLength();

            if (offset % 4 != 0) {
                report.addIssue(ValidationSeverity::Warning, "BUFFER_VIEW_ALIGNMENT",
                                "BufferView byteOffset (" + std::to_string(offset) + ") should be a multiple of 4.", bvPtr + "/byteOffset");
            }

            if (offset + length > bufLen) {
                report.addIssue(ValidationSeverity::Error, "BUFFER_VIEW_OUT_OF_BOUNDS",
                                "BufferView (" + std::to_string(offset) + " + " + std::to_string(length) + " = " +
                                std::to_string(offset + length) + " bytes) exceeds buffer capacity (" + std::to_string(bufLen) + " bytes).", bvPtr);
            }

            if (bv.getByteStride().has_value()) {
                uint32_t stride = bv.getByteStride().value();
                if (stride < 4 || stride > 252 || stride % 4 != 0) {
                    report.addIssue(ValidationSeverity::Error, "BUFFER_VIEW_INVALID_STRIDE",
                                    "byteStride (" + std::to_string(stride) + ") must be a multiple of 4 between 4 and 252.", bvPtr + "/byteStride");
                }
            }
        }
        bvIdx++;
    }

    // 3.3 Accessors
    auto& accessors = gltfModel.getElements<ux3d::gltf::Accessor>();
    int aIdx = 0;
    for (const auto& acc : accessors) {
        std::string accPtr = "/accessors/" + std::to_string(aIdx);
        uint32_t count = acc.getCount();
        if (count == 0) {
            report.addIssue(ValidationSeverity::Error, "ACCESSOR_EMPTY", "Accessor count cannot be 0.", accPtr + "/count");
        }

        uint32_t compSize = 1;
        switch (acc.getComponentType()) {
            case ux3d::gltf::ComponentType::Byte:
            case ux3d::gltf::ComponentType::UnsignedByte:   compSize = 1; break;
            case ux3d::gltf::ComponentType::Short:
            case ux3d::gltf::ComponentType::UnsignedShort:  compSize = 2; break;
            case ux3d::gltf::ComponentType::UnsignedInt:
            case ux3d::gltf::ComponentType::Float:          compSize = 4; break;
            default:
                report.addIssue(ValidationSeverity::Error, "ACCESSOR_INVALID_COMPONENT_TYPE", "Unrecognized accessor component type.", accPtr);
                break;
        }

        uint32_t numComp = 1;
        switch (acc.getType()) {
            case ux3d::gltf::AttributeType::Scalar: numComp = 1; break;
            case ux3d::gltf::AttributeType::Vec2:   numComp = 2; break;
            case ux3d::gltf::AttributeType::Vec3:   numComp = 3; break;
            case ux3d::gltf::AttributeType::Vec4:   numComp = 4; break;
            case ux3d::gltf::AttributeType::Mat2:   numComp = 4; break;
            case ux3d::gltf::AttributeType::Mat3:   numComp = 9; break;
            case ux3d::gltf::AttributeType::Mat4:   numComp = 16; break;
            default: break;
        }

        uint32_t elemSize = compSize * numComp;
        uint32_t accOffset = acc.hasByteOffset() ? acc.getByteOffset() : 0;

        if (accOffset % compSize != 0) {
            report.addIssue(ValidationSeverity::Error, "ACCESSOR_ALIGNMENT",
                            "Accessor byteOffset (" + std::to_string(accOffset) + ") must be a multiple of component size (" + std::to_string(compSize) + ").", accPtr + "/byteOffset");
        }

        const auto* bv = acc.getBufferView();
        if (bv != nullptr && count > 0) {
            uint32_t stride = bv->getByteStride().has_value() ? bv->getByteStride().value() : elemSize;
            uint32_t totalNeeded = (count - 1) * stride + elemSize;
            if (accOffset + totalNeeded > bv->getByteLength()) {
                report.addIssue(ValidationSeverity::Error, "ACCESSOR_OUT_OF_BOUNDS",
                                "Accessor range (" + std::to_string(accOffset) + " + " + std::to_string(totalNeeded) + " = " +
                                std::to_string(accOffset + totalNeeded) + " bytes) exceeds bufferView byteLength (" + std::to_string(bv->getByteLength()) + " bytes).", accPtr);
            }
        }

        // Min & Max validation
        const auto& minVec = acc.getMin();
        const auto& maxVec = acc.getMax();
        if (!minVec.empty() && !maxVec.empty()) {
            if (minVec.size() != maxVec.size() || minVec.size() != numComp) {
                report.addIssue(ValidationSeverity::Error, "ACCESSOR_MIN_MAX_DIMENSION_MISMATCH",
                                "Accessor min/max array dimensions (" + std::to_string(minVec.size()) + ") do not match component count (" + std::to_string(numComp) + ").", accPtr);
            } else {
                for (size_t i = 0; i < minVec.size(); ++i) {
                    if (minVec[i] > maxVec[i]) {
                        report.addIssue(ValidationSeverity::Error, "ACCESSOR_MIN_EXCEEDS_MAX",
                                        "Accessor min[" + std::to_string(i) + "] (" + std::to_string(minVec[i]) +
                                        ") is greater than max[" + std::to_string(i) + "] (" + std::to_string(maxVec[i]) + ").", accPtr);
                        break;
                    }
                }
            }
        }
        aIdx++;
    }

    // 3.4 Meshes & Primitives
    int mIdx = 0;
    for (auto* mesh : meshList) {
        std::string meshPtr = "/meshes/" + std::to_string(mIdx);
        if (mesh->getPrimitives().empty()) {
            report.addIssue(ValidationSeverity::Error, "MESH_NO_PRIMITIVES", "Mesh has no primitives defined.", meshPtr, SelectionType::Mesh, mIdx, mesh);
        }

        int pIdx = 0;
        for (const auto& prim : mesh->getPrimitives()) {
            std::string primPtr = meshPtr + "/primitives/" + std::to_string(pIdx);

            const ux3d::gltf::Accessor* posAcc = nullptr;
            for (const auto& attr : prim.getAttributes()) {
                if (attr.getName() == "POSITION") {
                    posAcc = attr.getAccessor();
                    break;
                }
            }

            if (posAcc == nullptr) {
                report.addIssue(ValidationSeverity::Error, "PRIMITIVE_MISSING_POSITION",
                                "Primitive missing required 'POSITION' attribute.", primPtr, SelectionType::Mesh, mIdx, mesh);
            } else {
                uint32_t vertexCount = posAcc->getCount();

                if (posAcc->getMin().empty() || posAcc->getMax().empty()) {
                    report.addIssue(ValidationSeverity::Warning, "POSITION_MISSING_MIN_MAX",
                                    "POSITION accessor should declare min and max bounds for frustum culling.", primPtr + "/attributes/POSITION", SelectionType::Mesh, mIdx, mesh);
                }

                // Parity check across all attributes
                for (const auto& attr : prim.getAttributes()) {
                    const auto* acc = attr.getAccessor();
                    if (acc != nullptr && acc->getCount() != vertexCount) {
                        report.addIssue(ValidationSeverity::Error, "ATTRIBUTE_COUNT_MISMATCH",
                                        "Attribute '" + std::string(attr.getName().c_str()) + "' count (" + std::to_string(acc->getCount()) +
                                        ") does not match POSITION count (" + std::to_string(vertexCount) + ").",
                                        primPtr + "/attributes/" + attr.getName().c_str(), SelectionType::Mesh, mIdx, mesh);
                    }
                }

                // Indices validation
                const auto* indAcc = prim.getIndicesAccessor();
                if (indAcc != nullptr) {
                    uint32_t indexCount = indAcc->getCount();
                    auto mode = prim.getPrimitiveMode();

                    if (mode == ux3d::gltf::PrimitiveMode::Triangles && (indexCount % 3 != 0)) {
                        report.addIssue(ValidationSeverity::Error, "INDEX_COUNT_NOT_DIVISIBLE_BY_3",
                                        "TRIANGLES primitive index count (" + std::to_string(indexCount) + ") is not a multiple of 3.",
                                        primPtr + "/indices", SelectionType::Mesh, mIdx, mesh);
                    } else if (mode == ux3d::gltf::PrimitiveMode::Lines && (indexCount % 2 != 0)) {
                        report.addIssue(ValidationSeverity::Error, "INDEX_COUNT_NOT_DIVISIBLE_BY_2",
                                        "LINES primitive index count (" + std::to_string(indexCount) + ") is not a multiple of 2.",
                                        primPtr + "/indices", SelectionType::Mesh, mIdx, mesh);
                    }

                    // Scan index buffer if available
                    core::MemoryRange<const uint8_t> indData;
                    if (indAcc->getData(indData) && !indData.empty()) {
                        uint32_t outOfRangeCount = 0;
                        uint32_t degenerateTriangleCount = 0;
                        auto compType = indAcc->getComponentType();

                        auto getIndexAt = [&](size_t i) -> uint32_t {
                            if (compType == ux3d::gltf::ComponentType::UnsignedByte) {
                                return indData[i];
                            } else if (compType == ux3d::gltf::ComponentType::UnsignedShort) {
                                uint16_t v;
                                std::memcpy(&v, indData.data() + i * 2, 2);
                                return v;
                            } else if (compType == ux3d::gltf::ComponentType::UnsignedInt) {
                                uint32_t v;
                                std::memcpy(&v, indData.data() + i * 4, 4);
                                return v;
                            }
                            return 0;
                        };

                        for (size_t i = 0; i < indexCount; ++i) {
                            uint32_t idxVal = getIndexAt(i);
                            if (idxVal >= vertexCount) {
                                outOfRangeCount++;
                            }
                        }

                        if (mode == ux3d::gltf::PrimitiveMode::Triangles && indexCount >= 3) {
                            for (size_t i = 0; i + 2 < indexCount; i += 3) {
                                uint32_t i0 = getIndexAt(i);
                                uint32_t i1 = getIndexAt(i + 1);
                                uint32_t i2 = getIndexAt(i + 2);
                                if (i0 == i1 || i1 == i2 || i0 == i2) {
                                    degenerateTriangleCount++;
                                }
                            }
                        }

                        if (outOfRangeCount > 0) {
                            report.addIssue(ValidationSeverity::Error, "INDEX_OUT_OF_RANGE",
                                            "Found " + std::to_string(outOfRangeCount) + " indices pointing beyond vertex count (" + std::to_string(vertexCount) + ").",
                                            primPtr + "/indices", SelectionType::Mesh, mIdx, mesh);
                        }

                        if (degenerateTriangleCount > 0) {
                            report.addIssue(ValidationSeverity::Warning, "DEGENERATE_TRIANGLES",
                                            "Primitive contains " + std::to_string(degenerateTriangleCount) + " degenerate (zero-area) triangles.",
                                            primPtr + "/indices", SelectionType::Mesh, mIdx, mesh);
                        }
                    }
                }
            }
            pIdx++;
        }
        mIdx++;
    }

    // 3.5 Materials
    int matIdx = 0;
    for (auto* mat : matList) {
        std::string matPtr = "/materials/" + std::to_string(matIdx);

        if (mat->getPbrMetallicRoughness().has_value()) {
            const auto& pbr = mat->getPbrMetallicRoughness().value();
            if (pbr.hasBaseColorFactor()) {
                auto col = pbr.getBaseColorFactorOrDefault();
                if (col.x < 0.0 || col.x > 1.0 || col.y < 0.0 || col.y > 1.0 ||
                    col.z < 0.0 || col.z > 1.0 || col.w < 0.0 || col.w > 1.0) {
                    report.addIssue(ValidationSeverity::Warning, "MATERIAL_COLOR_OUT_OF_RANGE",
                                    "BaseColorFactor values should be in range [0.0, 1.0].", matPtr + "/pbrMetallicRoughness/baseColorFactor", SelectionType::Material, matIdx, mat);
                }
            }

            if (pbr.hasMetallicFactor()) {
                double m = pbr.getMetallicFactorOrDefault();
                if (m < 0.0 || m > 1.0) {
                    report.addIssue(ValidationSeverity::Warning, "METALLIC_OUT_OF_RANGE",
                                    "metallicFactor must be in range [0.0, 1.0].", matPtr + "/pbrMetallicRoughness/metallicFactor", SelectionType::Material, matIdx, mat);
                }
            }

            if (pbr.hasRoughnessFactor()) {
                double r = pbr.getRoughnessFactorOrDefault();
                if (r < 0.0 || r > 1.0) {
                    report.addIssue(ValidationSeverity::Warning, "ROUGHNESS_OUT_OF_RANGE",
                                    "roughnessFactor must be in range [0.0, 1.0].", matPtr + "/pbrMetallicRoughness/roughnessFactor", SelectionType::Material, matIdx, mat);
                }
            }
        }
        matIdx++;
    }

    // 3.6 Animations
    int animIdx = 0;
    for (auto* anim : animList) {
        std::string animPtr = "/animations/" + std::to_string(animIdx);

        int sIdx = 0;
        for (const auto& sampler : anim->getSamplers()) {
            std::string samplerPtr = animPtr + "/samplers/" + std::to_string(sIdx);
            const auto* inAcc = sampler.getInputAccessor();
            const auto* outAcc = sampler.getOutputAccessor();

            if (inAcc == nullptr || outAcc == nullptr) {
                report.addIssue(ValidationSeverity::Error, "ANIMATION_SAMPLER_MISSING_ACCESSOR",
                                "Animation sampler missing input or output accessor.", samplerPtr, SelectionType::Animation, animIdx, anim);
            } else {
                if (inAcc->getComponentType() != ux3d::gltf::ComponentType::Float ||
                    inAcc->getType() != ux3d::gltf::AttributeType::Scalar) {
                    report.addIssue(ValidationSeverity::Error, "ANIMATION_INPUT_NOT_FLOAT_SCALAR",
                                    "Animation timeline input accessor must be SCALAR FLOAT.", samplerPtr + "/input", SelectionType::Animation, animIdx, anim);
                }

                // Check monotonicity of animation timeline
                core::MemoryRange<const uint8_t> inData;
                if (inAcc->getData(inData) && !inData.empty()) {
                    const float* times = reinterpret_cast<const float*>(inData.data());
                    size_t tCount = inAcc->getCount();
                    bool nonMonotonic = false;
                    for (size_t t = 1; t < tCount; ++t) {
                        if (times[t] <= times[t - 1]) {
                            nonMonotonic = true;
                            break;
                        }
                    }
                    if (nonMonotonic) {
                        report.addIssue(ValidationSeverity::Error, "ANIMATION_NON_MONOTONIC",
                                        "Animation timeline keyframe timestamps must be strictly increasing.", samplerPtr + "/input", SelectionType::Animation, animIdx, anim);
                    }
                }

                // CubicSpline count parity check
                if (sampler.getInterpolation() == ux3d::gltf::AnimationInterpolation::CubicSpline) {
                    if (outAcc->getCount() != inAcc->getCount() * 3) {
                        report.addIssue(ValidationSeverity::Error, "ANIMATION_CUBICSPLINE_COUNT_MISMATCH",
                                        "CubicSpline interpolation output count (" + std::to_string(outAcc->getCount()) +
                                        ") must be 3x input count (" + std::to_string(inAcc->getCount() * 3) + ").", samplerPtr + "/output", SelectionType::Animation, animIdx, anim);
                    }
                }
            }
            sIdx++;
        }

        int cIdx = 0;
        for (const auto& ch : anim->getChannels()) {
            std::string chPtr = animPtr + "/channels/" + std::to_string(cIdx);
            if (ch.getTarget().getNode() == nullptr) {
                report.addIssue(ValidationSeverity::Warning, "ANIMATION_CHANNEL_UNTARGETED",
                                "Animation channel does not specify a target node.", chPtr + "/target", SelectionType::Animation, animIdx, anim);
            }
            cIdx++;
        }
        animIdx++;
    }

    // 3.7 Scene Hierarchy & Cycle Detection
    std::unordered_map<const ux3d::gltf::Node*, int> parentCountMap;
    for (auto* node : nodeList) {
        for (auto* child : node->getChildren()) {
            if (child != nullptr) {
                parentCountMap[child]++;
            }
        }
    }

    // Check multiple parents
    for (auto* node : nodeList) {
        if (parentCountMap[node] > 1) {
            report.addIssue(ValidationSeverity::Error, "NODE_MULTIPLE_PARENTS",
                            "Node is referenced as a child by multiple parent nodes.",
                            "/nodes/" + std::to_string(std::distance(nodeList.begin(), std::find(nodeList.begin(), nodeList.end(), node))),
                            SelectionType::Node, (int)std::distance(nodeList.begin(), std::find(nodeList.begin(), nodeList.end(), node)), node);
        }
    }

    // Cycle detection via DFS
    std::unordered_set<const ux3d::gltf::Node*> visitedNodes;
    std::unordered_set<const ux3d::gltf::Node*> recursionStack;

    std::function<void(const ux3d::gltf::Node*)> checkNodeCycles = [&](const ux3d::gltf::Node* curr) {
        if (curr == nullptr) return;
        visitedNodes.insert(curr);
        recursionStack.insert(curr);

        for (auto* child : curr->getChildren()) {
            if (child == nullptr) continue;
            if (recursionStack.find(child) != recursionStack.end()) {
                report.addIssue(ValidationSeverity::Error, "SCENE_CYCLE_DETECTED",
                                "Circular hierarchy loop detected involving node: " + std::string(child->getName().c_str()),
                                "/nodes/" + std::to_string(std::distance(nodeList.begin(), std::find(nodeList.begin(), nodeList.end(), child))),
                                SelectionType::Node, (int)std::distance(nodeList.begin(), std::find(nodeList.begin(), nodeList.end(), child)), const_cast<ux3d::gltf::Node*>(child));
            } else if (visitedNodes.find(child) == visitedNodes.end()) {
                checkNodeCycles(child);
            }
        }

        recursionStack.erase(curr);
    };

    for (auto* node : nodeList) {
        if (visitedNodes.find(node) == visitedNodes.end() && parentCountMap[node] == 0) {
            checkNodeCycles(node);
        }
    }

    // Check orphan nodes
    for (size_t i = 0; i < nodeList.size(); ++i) {
        if (visitedNodes.find(nodeList[i]) == visitedNodes.end()) {
            report.addIssue(ValidationSeverity::Info, "NODE_ORPHAN",
                            "Node is not connected to any active scene hierarchy.",
                            "/nodes/" + std::to_string(i), SelectionType::Node, (int)i, nodeList[i]);
        }
    }

    // 3.8 Extensions (extensionsRequired vs extensionsUsed)
    if (jsonDoc.contains("extensionsRequired") && jsonDoc["extensionsRequired"].is_array()) {
        std::unordered_set<std::string> usedSet;
        if (jsonDoc.contains("extensionsUsed") && jsonDoc["extensionsUsed"].is_array()) {
            for (const auto& u : jsonDoc["extensionsUsed"]) {
                if (u.is_string()) usedSet.insert(u.get<std::string>());
            }
        }

        for (const auto& r : jsonDoc["extensionsRequired"]) {
            if (r.is_string()) {
                std::string req = r.get<std::string>();
                if (usedSet.find(req) == usedSet.end()) {
                    report.addIssue(ValidationSeverity::Error, "REQUIRED_EXTENSION_NOT_USED",
                                    "Extension '" + req + "' is declared in extensionsRequired but not in extensionsUsed.", "/extensionsRequired");
                }
            }
        }
    }

    // Correlate target objects with target indices for issues gathered in Tier 1
    for (auto& issue : report.issues) {
        if (issue.targetObject == nullptr && issue.targetIndex >= 0) {
            switch (issue.targetType) {
                case SelectionType::Node:
                    if (issue.targetIndex < (int)nodeList.size()) issue.targetObject = nodeList[issue.targetIndex];
                    break;
                case SelectionType::Material:
                    if (issue.targetIndex < (int)matList.size()) issue.targetObject = matList[issue.targetIndex];
                    break;
                case SelectionType::Mesh:
                    if (issue.targetIndex < (int)meshList.size()) issue.targetObject = meshList[issue.targetIndex];
                    break;
                case SelectionType::Animation:
                    if (issue.targetIndex < (int)animList.size()) issue.targetObject = animList[issue.targetIndex];
                    break;
                case SelectionType::Camera:
                    if (issue.targetIndex < (int)camList.size()) issue.targetObject = camList[issue.targetIndex];
                    break;
                case SelectionType::Skin:
                    if (issue.targetIndex < (int)skinList.size()) issue.targetObject = skinList[issue.targetIndex];
                    break;
                case SelectionType::Texture:
                    if (issue.targetIndex < (int)texList.size()) issue.targetObject = texList[issue.targetIndex];
                    break;
                case SelectionType::Image:
                    if (issue.targetIndex < (int)imgList.size()) issue.targetObject = imgList[issue.targetIndex];
                    break;
                default: break;
            }
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    report.durationMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
    report.validated = true;

    m_lastReport = report;
    return report;
}

} // namespace ux3d::imgui
