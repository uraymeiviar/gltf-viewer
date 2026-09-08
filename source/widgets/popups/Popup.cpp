#include "widgets/popups/Popup.h"
#include "helpers/Utils.h"

#include <QApplication>
#include <QFrame>
#include <QVBoxLayout>

using namespace ux3d::editor;

Popup::Popup(QWidget* parent, QPoint position, bool closeOnFocusLoss, bool windowFrame, int entryWidth, int popupWidth)
    : QWidget(parent)
    , m_popupEntryWidth(entryWidth)
{
    {
        Qt::WindowFlags flags = Qt::Dialog;
        if (!windowFrame) {
            flags |= Qt::FramelessWindowHint;
        }
        // keep popups always on top
        flags |= Qt::WindowStaysOnTopHint;
        setWindowFlags(flags);
    }

    connect(static_cast<QApplication*>(QApplication::instance()), &QGuiApplication::applicationStateChanged, this, [this]() {
        if (!isVisible()) {
            return;
        }
        if (QGuiApplication::applicationState() == Qt::ApplicationActive) {
            setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);
        } else {
            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
        }
        show();
    });

    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName("Popup");
    setFocus();
    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(0, 0, 0, 0);
    m_frame = new QFrame(this);
    m_frame->setObjectName("Popup_Frame");
    m_popupLayout = new QVBoxLayout();

    m_entryLayout = new QVBoxLayout();
    m_entryLayout->setContentsMargins(10, 0, 10, 0);
    m_entryLayout->setSpacing(0);
    m_popupLayout->addLayout(m_entryLayout);
    m_popupLayout->addSpacing(20);

    m_frame->setLayout(m_popupLayout);
    mainLayout->addWidget(m_frame);

    setLayout(mainLayout);
    setFixedWidth(popupWidth);
    setObjectName("Popup_Widget");

    // Note that the final position is determined by showEvent()
    move(position);

    if (closeOnFocusLoss) {
        connect(static_cast<QApplication*>(QApplication::instance()), &QApplication::focusChanged, this, [this](QWidget*, QWidget* now) {
            while (now != nullptr && now != this) {
                now = dynamic_cast<QWidget*>(now->parent());
            }
            if (this != now) {
                close();
            }
        });
    }
}

void Popup::addWidget(QWidget* entry, int position)
{
    m_entryLayout->insertWidget(position, entry);
}

void Popup::showEvent(QShowEvent*)
{
    utils::moveToAvailableSpace(this);
    setFixedHeight(sizeHint().height());
}
