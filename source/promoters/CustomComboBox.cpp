#include "promoters/CustomComboBox.h"
#include "Constants.h"
#include "widgets/ExtensionLabel.h"
#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QStylePainter>

using namespace ux3d::editor;

CustomComboBox::CustomComboBox(QWidget* _parent)
    : QComboBox(_parent)
{
    setFocusPolicy(Qt::StrongFocus);
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int _index) {
        addExtensionLabelToComboBox(_index);
    });
}

void CustomComboBox::wheelEvent(QWheelEvent* event)
{
    if (!hasFocus()) {
        event->ignore();
    } else {
        QComboBox::wheelEvent(event);
    }
}

void CustomComboBox::paintEvent(QPaintEvent* event)
{
    // We override this paint event to fix the known qt bug that placeholder text is not displayed
    QStylePainter painter(this);
    painter.setPen(palette().color(QPalette::Text));

    // draw the combobox frame, focusrect and selected etc.
    QStyleOptionComboBox opt;
    initStyleOption(&opt);
    painter.drawComplexControl(QStyle::CC_ComboBox, opt);

    if (currentIndex() < 0) {
        opt.palette.setBrush(QPalette::ButtonText, opt.palette.brush(QPalette::ButtonText).color().lighter());

        // our custom code - begin
        if (placeholderText() != "") {
            opt.currentText = placeholderText();
        }
        // our custom code - end
    }

    // draw the icon and text
    painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
}

void CustomComboBox::addExtensionLabelToItem(int _index, ExtensionLabel* _label)
{
    QWidget* widget = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(widget);
    widget->setLayout(layout);
    layout->addSpacing(fontMetrics().horizontalAdvance(itemText(_index)) + constants::labelRightPadding);
    layout->setContentsMargins(0, 0, 0, 0);
    widget->setMouseTracking(true);
    layout->addWidget(_label);
    layout->addStretch();
    view()->setIndexWidget(model()->index(_index, 0), widget);
}

void CustomComboBox::addExtensionLabelToComboBox(int _index)
{
    QHBoxLayout* comboBoxLayout = static_cast<QHBoxLayout*>(this->layout());
    // remove extension label
    if (m_comBoBoxExtensionLabel != nullptr) {
        comboBoxLayout->removeWidget(m_comBoBoxExtensionLabel);
        delete m_comBoBoxExtensionLabel;
        m_comBoBoxExtensionLabel = nullptr;
    }
    // clean all spacers, stretch objects, etc.
    delete comboBoxLayout;
    comboBoxLayout = new QHBoxLayout(this);
    comboBoxLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(comboBoxLayout);

    // add extension label again if necessary
    if (QWidget* entryWidget = view()->indexWidget(model()->index(_index, 0));
        entryWidget != nullptr) {
        comboBoxLayout->addSpacing(fontMetrics().horizontalAdvance(itemText(_index)) + constants::labelRightPadding);
        ExtensionLabel* label = entryWidget->findChild<ExtensionLabel*>();
        if (label != nullptr) {
            m_comBoBoxExtensionLabel = label->clone(this);
            comboBoxLayout->addWidget(m_comBoBoxExtensionLabel);
        }
        comboBoxLayout->addStretch();
    }
}
