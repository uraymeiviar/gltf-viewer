#include "promoters/GltfPropertyComboBox.h"

void ux3d::editor::GltfPropertyComboBox::setProperty(const QString& _text, GltfPropertyConstPointer _property)
{
    int index = indexOf(_property);
    if (index < 0) {
        addItem(_text, QVariant::fromValue(_property));
    } else {
        setItemText(index, _text);
    }
}

void ux3d::editor::GltfPropertyComboBox::removeProperty(GltfPropertyConstPointer _property)
{
    int index = indexOf(_property);
    if (index >= 0) {
        removeItem(index);
    }
}

ux3d::editor::GltfPropertyConstPointer ux3d::editor::GltfPropertyComboBox::getPropertyAtIndex(int _index) const
{
    QVariant data = itemData(_index);
    if (data.canConvert<GltfPropertyConstPointer>()) {
        return data.value<GltfPropertyConstPointer>();
    }

    return { /* none */ };
}

int ux3d::editor::GltfPropertyComboBox::indexOf(GltfPropertyConstPointer _property) const
{
    for (int i = 0; i < count(); ++i) {
        GltfPropertyConstPointer property = getPropertyAtIndex(i);
        if (property == _property) {
            return i;
        }
    }

    return ~0;
}
