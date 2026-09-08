// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "Ktx2.h"
#include "XMP.h"
#include "core/HeapAllocator.h"
#include "filesystem/Dir.h"
#include "gltf/StorageFormats.h"
#include "image/Image.h"
#include "imageformats/imageformats.h"
#include <optional>

// See https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#image

namespace ux3d::gltf {
class BufferView;

class Image : public ChildOfRoot {
public:
    Image(uint32_t _id); // regular constructor
    Image(uint32_t _id, const Image& _source); // duplicate in same glTF

    Image(const Image&) = delete;
    Image(Image&&) = delete;
    void operator=(const Image&) = delete;
    void operator=(Image&&) = delete;
    ~Image() override = default;

    GltfType getGltfType() const override { return GltfType::Image; }

    const core::string& getUri() const { return m_uri; }
    GltfResult setUri(const core::string& _uri);

    imageformats::ImageEncoding getMimeType() const { return m_mimeType; }
    GltfResult setMimeType(imageformats::ImageEncoding _mimeType);

    BufferView* getBufferView() { return m_bufferView; }
    const BufferView* getBufferView() const { return m_bufferView; }
    GltfResult setBufferView(BufferView& _bufferView);
    void unsetBufferView() { m_bufferView = nullptr; }
    uint32_t m_bufferViewIndex;

    bool hasXMPPacket() const { return m_xmpPacket.has_value(); }
    std::optional<PacketContainer>& getXMPPacket() { return m_xmpPacket; }
    const std::optional<PacketContainer>& getXMPPacket() const { return m_xmpPacket; }
    GltfResult setXMPPacket(Packet* _xmpPacket);
    GltfResult unsetXMPPacket();

    const image::Image& getData() const { return m_data; }
    GltfResult setData(image::Image&& _data);

    const core::Vector<uint8_t>* getEncodedBytes() const { return &m_encodedData; }
    GltfResult setEncodedBytes(const core::Vector<uint8_t>& _data)
    {
        encodedMapID = encodedMapID != -1 ? encodedMapID + 1 : 0;
        m_encodedData = _data;
        m_historyDataIsDirty = true;
        return GltfResult::Success;
    }

    uint64_t getSizeInBytes();
    GltfResult loadDataToMemory(const std::optional<filesystem::Dir>& _gltfDir);
    GltfResult reloadDataToMemory(const filesystem::File& _imageFile);

    StorageFormat getStorageFormat() const { return m_storageFormat; }
    void setStorageFormat(StorageFormat _storageFormat)
    {
        m_storageFormat = _storageFormat;
        m_historyDataIsDirty = true;
    }
    void convertToStorageFormat(StorageFormat _storageFormat);

    core::Vector<uint32_t> getResolution() const;

    bool hasTextureChangedForRenderer();
    void textureChangedForRenderer() { m_imageChangedForRenderer = true; }

    void changeUriToCorrectFileExtension(imageformats::ImageEncoding _mimeType);
    std::string createUniqueURI(const gltf::Gltf& gltf);
    std::string createURI();

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const override;

    void loadAsErrorImage();

    static bool isValidEncoding(imageformats::ImageEncoding _encoding);

public:
    image::Image m_data = image::Image();

    // yes this is a crude hack, for until we remove the gltf module alltogether.
    // Reason: we somehow need to track if the data has changed to be able to only
    // store changed data in the undo history's data store
    bool m_historyDataIsDirty = true;

    // TODO: This is a blatant hack that should be removed when textures are truly only recreated when glTF images/textures change.
    // TODO: Currently, *any* change including uniform changes during translations lead to a full glTF -> renderer update, including texture uploads.
    int encodedMapID = -1;

private:
    GltfResult loadDataFromBufferView();
    GltfResult loadDataFromUri(const filesystem::Dir& _gltfDir);
    GltfResult loadDataFromEmbedded();
    bool isValidBasisUTexture() const;
    bool isValidBasisUDimensions() const;
    bool isValidBasisUColorSpace() const;
    bool isValidBasisUCompression() const;

    // actual glTF Image data
    core::string m_uri;
    BufferView* m_bufferView = nullptr;
    imageformats::ImageEncoding m_mimeType = imageformats::ImageEncoding::Undefined;

    // glTF extensions
    std::optional<PacketContainer> m_xmpPacket = std::nullopt;

    // helpers
    StorageFormat m_storageFormat = StorageFormat::Undefined;
    static uint32_t m_uniqueIdCounter;
    bool m_imageChangedForRenderer = false;
    core::Vector<uint8_t> m_encodedData;
};
}
