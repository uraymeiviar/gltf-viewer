#pragma once

#include "GltfPropertyPointer.h"
#include "promoters/CustomComboBox.h"

namespace ux3d::editor {
class GltfPropertyComboBox final : public CustomComboBox {
public:
    using CustomComboBox::CustomComboBox;
    ~GltfPropertyComboBox() final = default;

    void setProperty(const QString& _text, GltfPropertyConstPointer _property);
    void removeProperty(GltfPropertyConstPointer _property);
    GltfPropertyConstPointer getPropertyAtIndex(int _index) const;

private:
    int indexOf(GltfPropertyConstPointer _property) const;
};
}

Q_DECLARE_METATYPE(ux3d::editor::GltfPropertyConstPointer)
