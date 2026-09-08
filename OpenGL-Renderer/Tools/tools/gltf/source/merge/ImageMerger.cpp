// Copyright (c) 2021 UX3D GmbH. All rights reserved.

#include "gltf/merge/ImageMerger.h"
#include "gltf/Gltf.h"
#include "gltf/IndexLookup.h"

using namespace ux3d;
using namespace ux3d::gltf;

bool ImageMerger::resolveDependencies(const Image& _srcImage, Image& _dstImage)
{
    resolveXMP(_srcImage, _dstImage);

    if (_srcImage.getBufferView() == nullptr) {
        return true;
    }

    const size_t bufferViewIndex = m_indices[_srcImage.getBufferView()];
    BufferView& bufferView = m_destination.getBufferViews().at(bufferViewIndex);
    _dstImage.setBufferView(bufferView);

    return true;
}

void ImageMerger::resolveXMP(const Image& source, Image& dest)
{
    if (source.hasXMPPacket()) {
        const Packet* pSourcePacket = source.getXMPPacket().value().m_packet;
        const size_t packetIndex = m_indices[pSourcePacket];

        if (m_destination.hasXMP() == false) {
            m_destination.createXMP();
        }

        dest.setXMPPacket(&(m_destination.getXMP().value().getPackets().at(packetIndex)));
    }
}
