#include "widgets/ArbitraryTableHeader.h"

using namespace ux3d;
using namespace ux3d::editor;

void ArbitraryTableHeader::showEvent(QShowEvent* _event)
{
    for (int i = 0; i < count(); i++) {
        m_sectionWidgets[i]->setGeometry(sectionViewportPosition(i), 0, sectionSize(i), height());
    }

    QWidget::showEvent(_event);
}

void ArbitraryTableHeader::setSectionWidget(int _sectionIndex, QWidget* _widget)
{
    if (_sectionIndex >= count()) {
        return;
    }

    m_sectionWidgets.reserve(count());
    for (int i = 0; i < count(); i++) {
        if (i >= m_sectionWidgets.size()) {
            m_sectionWidgets.push_back(nullptr);
        }
    }

    _widget->setParent(this);
    delete m_sectionWidgets[_sectionIndex];
    m_sectionWidgets[_sectionIndex] = _widget;
}

void ArbitraryTableHeader::setModel(QAbstractItemModel* _model)
{
    for (QWidget* widget : m_sectionWidgets) {
        delete widget;
    }

    m_sectionWidgets.clear();
    QHeaderView::setModel(_model);
}
