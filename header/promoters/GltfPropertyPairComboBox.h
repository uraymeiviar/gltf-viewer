#pragma once

#include "GltfPropertyPointer.h"
#include "promoters/CustomComboBox.h"

namespace ux3d::editor {
using GltfPropertyPair = std::pair<GltfPropertyConstPointer, GltfPropertyConstPointer>;

class GltfPropertyPairComboBox final : public QComboBox {
    Q_OBJECT
public:
    using QComboBox::QComboBox;
    ~GltfPropertyPairComboBox() final = default;

    int setPair(const QString& _text, GltfPropertyConstPointer _a, GltfPropertyConstPointer _b);
    void removePair(GltfPropertyConstPointer _a, GltfPropertyConstPointer _b);
    GltfPropertyPair getPairAtIndex(int _index) const;
    int indexOf(const GltfPropertyPair& _pair) const;

    void removeAll(GltfPropertyConstPointer _gltfPointer);
    void updateAll(GltfPropertyConstPointer _gltfPointer, const std::function<QString(GltfPropertyPair _pair)>& _makeText);
};
}

Q_DECLARE_METATYPE(ux3d::editor::GltfPropertyPair)
