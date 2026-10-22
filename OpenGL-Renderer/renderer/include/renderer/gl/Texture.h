// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "image/Image.h"
#include "renderer/gl/Enums.h"
#include "renderer/gl/OpenGL.h"

#include <vector>

namespace ux3d::renderer::gl {

class GL;

class Texture {
public:
    Texture(GL*, TextureTarget target);
    // Borrowed handles remain owned by their framebuffer; moves preserve ownership.
    Texture(GL*, GLuint handle, TextureTarget target, bool ownsHandle = true);
    Texture(GL*, const image::Image& image);
    Texture(Texture&&);
    ~Texture();

    void bind() const;

    void write(size_t width, size_t height, Format format, ComponentType pixelFormat, const void* pixels, InternalFormat internalFormat = InternalFormat::Identity);

    void write(const image::Image& image, bool isLinear);

    static Format extractFormat(const image::Image& image);
    static ComponentType extractComponentType(const image::Image& image);
    static TextureTarget extractTarget(const image::Image& image);

    const TextureTarget target;
    GLuint handle = invalid;

    // TODO: This is a blatant hack that should be removed when textures are truly only recreated when glTF images/textures change.
    // TODO: Currently, *any* change including uniform changes during translations lead to a full glTF -> renderer update, including texture uploads.
    int currentMapID = -1;

private:
    GL* m_gl;
    bool m_ownsHandle = true;
};

}
