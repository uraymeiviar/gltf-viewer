#include "gltf/json/ScenesToJson.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"
#include "gltf/Scene.h"
#include "gltf/XMP.h"

using namespace ux3d;
using namespace ux3d::gltf;

ScenesToJson::ScenesToJson(const IndexLookup& _indices, const ExportOptions& _exportOptions, const Gltf& _gltf)
    : ChildOfRootToJson(_indices, _exportOptions)
    , m_gltf(_gltf)
{
}

void ScenesToJson::convert(const Scene& _scene, nlohmann::json& _jsonScene, const JsonPath& _jsonPath) const
{
    convertBase(_scene, _jsonScene);
    convertNodes(_scene, _jsonScene);
    convertXMP(_scene, _jsonScene);
    convertAudio(_scene, _jsonScene);
}

void ScenesToJson::convertNodes(const Scene& _scene, nlohmann::json& _jsonScene) const
{
    if (!_scene.getNodes().empty()) {
        nlohmann::json jsonNodes = nlohmann::json::array();

        for (const Node* pNode : _scene.getNodes()) {
            size_t index = m_indices[pNode];
            jsonNodes.emplace_back(index);
        }

        _jsonScene["nodes"] = jsonNodes;
    }
}

void ScenesToJson::convertXMP(const Scene& _scene, nlohmann::json& _jsonScene) const
{
    if (_scene.hasXMPPacket()) {
        const PacketContainer& container = _scene.getXMPPacket().value();
        Packet* pPacket = container.m_packet;
        if (m_gltf.getXMP().has_value()) {
            const XMP& xmp = m_gltf.getXMP().value();
            const core::LinkedList<Packet>& packets = xmp.getPackets();

            for (size_t i = 0; i < packets.size(); ++i) {
                if (&packets.at(i) == pPacket) {
                    _jsonScene["extensions"]["KHR_xmp_json_ld"]["packet"] = i;
                    break;
                }
            }
        }

        // convert extras and extensions
        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(container, _jsonScene["extensions"]["KHR_xmp_json_ld"]);
        gltfObjectToJson.convertUnknownExtensions(container, _jsonScene["extensions"]["KHR_xmp_json_ld"]);
    }
}

void ScenesToJson::convertAudio(const Scene& _scene, nlohmann::json& _jsonScene) const
{
    if (_scene.hasAudio()) {
        nlohmann::json jsonAudio = nlohmann::json::object();

        GltfObjectToJson gltfObjectToJson(m_exportOptions);
        gltfObjectToJson.convertExtras(_scene.getAudioEmitterPointerObject().value(), jsonAudio);
        gltfObjectToJson.convertUnknownExtensions(_scene.getAudioEmitterPointerObject().value(), jsonAudio);

        if (_scene.hasAudioEmitters()) {
            nlohmann::json jsonEmitters = nlohmann::json::array();
            for (AudioEmitter* emitter : _scene.getAudioEmitters()) {
                size_t index = m_indices[emitter];
                jsonEmitters.push_back(index);
            }
            jsonAudio["emitters"] = jsonEmitters;
        }

        _jsonScene["extensions"]["KHR_audio"] = jsonAudio;
    }
}
