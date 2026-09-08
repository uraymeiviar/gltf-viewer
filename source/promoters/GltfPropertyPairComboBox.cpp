#include "promoters/GltfPropertyPairComboBox.h"

int ux3d::editor::GltfPropertyPairComboBox::setPair(const QString& _text, GltfPropertyConstPointer _a, GltfPropertyConstPointer _b)
{
    GltfPropertyPair pair = GltfPropertyPair(_a, _b);

    int index = indexOf(pair);
    if (index < 0) {
        index = count();
        addItem(_text, QVariant::fromValue(pair));
    } else {
        setItemText(index, _text);
    }

    return index;
}

void ux3d::editor::GltfPropertyPairComboBox::removePair(GltfPropertyConstPointer _a, GltfPropertyConstPointer _b)
{
    GltfPropertyPair pair(_a, _b);
    int index = indexOf(pair);
    if (index >= 0) {
        removeItem(index);
    }
}

void ux3d::editor::GltfPropertyPairComboBox::removeAll(GltfPropertyConstPointer _gltfPointer)
{
    for (int i = count() - 1; i >= 0; --i) {
        GltfPropertyPair pair = getPairAtIndex(i);
        if (pair.first == _gltfPointer || pair.second == _gltfPointer) {
            removeItem(i);
        }
    }
}

void ux3d::editor::GltfPropertyPairComboBox::updateAll(GltfPropertyConstPointer _gltfPointer, const std::function<QString(GltfPropertyPair _pair)>& _makeText)
{
    for (int i = 0; i < count(); ++i) {
        GltfPropertyPair pair = getPairAtIndex(i);
        if (pair.first == _gltfPointer || pair.second == _gltfPointer) {
            setItemText(i, _makeText(pair));
        }
    }
}

ux3d::editor::GltfPropertyPair ux3d::editor::GltfPropertyPairComboBox::getPairAtIndex(int _index) const
{
    QVariant data = itemData(_index);
    if (data.canConvert<GltfPropertyPair>()) {
        return data.value<GltfPropertyPair>();
    }

    return { /* empty pair */ };
}

int ux3d::editor::GltfPropertyPairComboBox::indexOf(const GltfPropertyPair& _pair) const
{
    for (int i = 0; i < count(); ++i) {
        GltfPropertyPair pair = getPairAtIndex(i);
        if (pair.first == _pair.first && pair.second == _pair.second) {
            return i;
        }
    }

    return -1;
}
