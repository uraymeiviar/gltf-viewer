#include "gltf/json/NodesToJson.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/Node.h"
#include "gltf/XMP.h"
#include "gltf/json/GltfObjectToJson.h"

using namespace ux3d;
using namespace ux3d::gltf;

NodesToJson::NodesToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions, const Gltf& _gltf)
    : ChildOfRootToJson(_indices, _exportOptions)
    , m_gltf(_gltf)
{
}

void NodesToJson::convert(const Node& _node, nlohmann::json& _jsonNode, const JsonPath& _jsonPath) const
{
    convertBase(_node, _jsonNode);
    convertCamera(_node, _jsonNode);
    convertChildren(_node, _jsonNode);
    convertSkin(_node, _jsonNode);
    convertMatrix(_node, _jsonNode);
    convertMesh(_node, _jsonNode);
    convertRotation(_node, _jsonNode);
    convertScale(_node, _jsonNode);
    convertTranslation(_node, _jsonNode);
    convertWeights(_node, _jsonNode);

    // extensions
    convertLight(_node, _jsonNode);
    convertXMP(_node, _jsonNode);
    convertAudio(_node, _jsonNode);
}

void NodesToJson::convertCamera(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.getCamera() != nullptr) {
        size_t index = m_indices[_node.getCamera()];
        _jsonNode["camera"] = index;
    }
}

void NodesToJson::convertChildren(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (!_node.getChildren().empty()) {
        nlohmann::json jsonChildren = nlohmann::json::array();

        for (const Node* pNode : _node.getChildren()) {
            size_t index = m_indices[pNode];
            jsonChildren.emplace_back(index);
        }

        _jsonNode["children"] = jsonChildren;
    }
}

void NodesToJson::convertSkin(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.getSkin() != nullptr) {
        size_t index = m_indices[_node.getSkin()];
        _jsonNode["skin"] = index;
    }
}

void NodesToJson::convertMatrix(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.getStoreAsMatrix()) {
        if (_node.hasRotation() || _node.hasTranslation() || _node.hasScale()) {
            if (_node.isAnimationTarget(m_gltf)) {
                core::logInfo("node.%s transform is stored as TRS components since it is an animation target", _node.getName().c_str());
                return;
            }
            math::dmat4x4 matrix = _node.getTransformMatrix();
            nlohmann::json jsonMatrix = nlohmann::json::array();

            // column-major order
            for (int32_t x = 0; x < 4; x++) {
                for (int32_t y = 0; y < 4; y++) {
                    jsonMatrix.emplace_back(matrix[x][y]);
                }
            }

            _jsonNode["matrix"] = jsonMatrix;
        }
    }
}

void NodesToJson::convertMesh(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.getMesh() != nullptr) {
        size_t index = m_indices[_node.getMesh()];
        _jsonNode["mesh"] = index;
    }
}

void NodesToJson::convertRotation(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.hasRotation() && (!_node.getStoreAsMatrix() || _node.isAnimationTarget(m_gltf))) {
        math::dquat rotation = _node.getRotationOrDefault();
        nlohmann::json jsonArray = nlohmann::json::array();

        for (int32_t i = 0; i < 4; i++) {
            jsonArray.emplace_back(rotation[i]);
        }

        _jsonNode["rotation"] = jsonArray;
    }
}

void NodesToJson::convertScale(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.hasScale() && (!_node.getStoreAsMatrix() || _node.isAnimationTarget(m_gltf))) {
        math::dvec3 scale = _node.getScaleOrDefault();
        nlohmann::json jsonArray = nlohmann::json::array();

        for (int32_t i = 0; i < 3; i++) {
            jsonArray.emplace_back(scale[i]);
        }

        _jsonNode["scale"] = jsonArray;
    }
}

void NodesToJson::convertTranslation(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.hasTranslation() && (!_node.getStoreAsMatrix() || _node.isAnimationTarget(m_gltf))) {
        math::dvec3 translation = _node.getTranslationOrDefault();
        nlohmann::json jsonArray = nlohmann::json::array();

        for (int32_t i = 0; i < 3; i++) {
            jsonArray.emplace_back(translation[i]);
        }

        _jsonNode["translation"] = jsonArray;
    }
}

void NodesToJson::convertWeights(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.weights && !_node.weights->empty()) {
        nlohmann::json jsonArray = nlohmann::json::array();

        for (double weight : *_node.weights) {
            jsonArray.emplace_back(weight);
        }

        _jsonNode["weights"] = jsonArray;
    }
}

// Extensions:
void NodesToJson::convertLight(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.getLight() != nullptr) {
        size_t index = m_indices[_node.getLight()];

        nlohmann::json jsonLightsPunctual = nlohmann::json::object();
        if (_node.getLightPointerObject().has_value()) {
            GltfObjectToJson gltfObjectToJson(m_exportOptions);
            gltfObjectToJson.convertExtras(_node.getLightPointerObject().value(), jsonLightsPunctual);
            gltfObjectToJson.convertUnknownExtensions(_node.getLightPointerObject().value(), jsonLightsPunctual);
        }

        jsonLightsPunctual["light"] = index;

        _jsonNode["extensions"]["KHR_lights_punctual"] = jsonLightsPunctual;
    }
}

void NodesToJson::convertXMP(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.hasXMPPacket()) {
        const PacketContainer& container = _node.getXMPPacket().value();
        Packet* pPacket = container.m_packet;
        if (m_gltf.getXMP().has_value()) {
            const XMP& xmp = m_gltf.getXMP().value();
            const core::LinkedList<Packet>& packets = xmp.getPackets();

            for (size_t i = 0; i < packets.size(); ++i) {
                if (&packets.at(i) == pPacket) {
                    _jsonNode["extensions"]["KHR_xmp_json_ld"]["packet"] = i;
                    break;
                }
            }
        }

        // convert extras and extensions
        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(container, _jsonNode["extensions"]["KHR_xmp_json_ld"]);
        gltfObjectToJson.convertUnknownExtensions(container, _jsonNode["extensions"]["KHR_xmp_json_ld"]);
    }
}

void NodesToJson::convertAudio(const Node& _node, nlohmann::json& _jsonNode) const
{
    if (_node.hasAudio()) {
        nlohmann::json jsonAudio = nlohmann::json::object();

        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(_node.getAudioEmitterPointerObject().value(), jsonAudio);
        gltfObjectToJson.convertUnknownExtensions(_node.getAudioEmitterPointerObject().value(), jsonAudio);

        if (_node.hasAudioEmitter()) {

            size_t index = m_indices[_node.getAudioEmitter()];
            jsonAudio["emitter"] = index;
        }

        _jsonNode["extensions"]["KHR_audio"] = jsonAudio;
    }
}