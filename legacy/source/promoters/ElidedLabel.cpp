#include "promoters/ElidedLabel.h"
#include <QPainter>
#include <QSize>
#include <QSizePolicy>
#include <QTextLayout>

using namespace ux3d;
using namespace ux3d::editor;

ElidedLabel::ElidedLabel(const QString& text, QWidget* parent)
    : QFrame(parent)
    , content(text)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

ElidedLabel::ElidedLabel(QWidget* parent)
    : ElidedLabel("", parent)
{
}

void ElidedLabel::setText(const QString& newText)
{
    content = newText;
    update();
}

QSize ElidedLabel::minimumSizeHint() const
{
    QFontMetrics metric = fontMetrics();
    int minimumWidth = content.isEmpty() ? 0 : metric.horizontalAdvance(content[0] + QString("...")) + 1;
    return QSize(minimumWidth, metric.height());
}

void ElidedLabel::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    QFontMetrics metric = fontMetrics();
    setMaximumWidth(metric.horizontalAdvance(content) + 1);
    if (!content.isEmpty()) {
        setMinimumWidth(metric.horizontalAdvance(content[0] + QString("...")) + 1);
    }
    setMinimumHeight(metric.height());
}

void ElidedLabel::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);

    QPainter painter(this);
    QFontMetrics fontMetrics = painter.fontMetrics();

    QTextLayout textLayout(content, painter.font());
    textLayout.beginLayout();
    QTextLine line = textLayout.createLine();

    line.setLineWidth(width());

    QString lastLine = content.mid(line.textStart());
    QString elidedLastLine = fontMetrics.elidedText(lastLine, Qt::ElideRight, width());
    painter.drawText(QPoint(0, fontMetrics.ascent()), elidedLastLine);

    textLayout.endLayout();
}
