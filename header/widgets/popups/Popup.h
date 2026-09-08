#pragma once

#include "Constants.h"
#include <QWidget>

class QVBoxLayout;
class QShowEvent;
class QFrame;

namespace ux3d::editor {
class Entry;
class Popup : public QWidget {
    Q_OBJECT
public:
    Popup(QWidget* parent, QPoint position, bool closeOnFocusLoss, bool windowFrame, int entryWidth = constants::popupEntryWidth, int popupWidth = constants::popupWidth);
    virtual ~Popup() = default;
    void addWidget(QWidget* widget, int position = -1);

protected:
    void showEvent(QShowEvent* event) override;

    QVBoxLayout* m_popupLayout;
    QVBoxLayout* m_entryLayout;
    QFrame* m_frame;

    int m_popupEntryWidth;

private:
    QPoint m_oldPos;
};
}
