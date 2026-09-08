// Copyright © 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "GltfPropertyPointer.h"

#include <string>

namespace ux3d::gltf {
class ChildOfRoot;
class Scene;
class Image;
class Texture;
class Primitve;
class Packet;
}

namespace ux3d::editor {
class Name {
public:
    explicit Name(const gltf::ChildOfRoot& _object);
    explicit Name(GltfPropertyConstPointer _object);
    explicit Name(GltfPropertyPointer _object);
    operator const std::string&() const { return forDisplay(); }

    const std::string& raw() const { return m_realName; }
    const std::string& forDisplay() const { return m_displayName; }
    const std::string& forInspectorDisplay() const { return m_displayNameInspector; }
    const std::string& forEdit() const { return m_undecoratedDisplayName; }
    bool unnamed() const { return m_realName.empty(); }

private:
    std::string m_realName;
    std::string m_displayName;
    std::string m_displayNameInspector;
    std::string m_undecoratedDisplayName;

    void fillInfo(const gltf::ChildOfRoot& _object);
    void fillInfo(const gltf::Primitive& _primitive);
    void fillInfo(const gltf::Packet& _packet);

    static std::string getDisplayName(const gltf::ChildOfRoot& _object);
    static std::string getID(const gltf::ChildOfRoot& _object);
    static std::string getDisplayNamePrefix(const gltf::ChildOfRoot& _object);
    void postProcess(const gltf::ChildOfRoot& _object);
    void scenePostProcess(const gltf::Scene& _scene);
    void imagePostProcess(const gltf::Image& _image);
    void texturePostProcess(const gltf::Texture& _texture);
    void audioSourcePostProcess(const gltf::AudioSource& source);
    void postProcessInspectorDisplay();
    static std::string nameFromUri(const std::string& _uri);
};
}
