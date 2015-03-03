#include "qcircleprogressbar.h"

#include <QStylePainter>
#include <QDebug>

QCircleProgressBar::QCircleProgressBar(QWidget *parent) :
    QProgressBar(parent), transparentCenter(false), startAngle(-90.0)
{
    this->setGeometry(0, 0, 100, 100);
    this->setStyleSheet("font-size: 20px; color: #efefef;");
}

void QCircleProgressBar::paintEvent(QPaintEvent * /*event*/)
{
    //TODO
    static double coefOuter = 0.66;
    static double coefInner = 0.55;
    static double penSize = 0.0;
    static QColor borderColor(86, 88, 91);
    static QColor grooveColor(86, 88, 91);
    static QColor chunkColor(0, 172, 227);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRectF outerRect(rect().x()*coefOuter, rect().y()*coefOuter, rect().width()*coefOuter, rect().height()*coefOuter);
    QRectF innerRect(rect().x()*coefInner, rect().y()*coefInner, rect().width()*coefInner, rect().height()*coefInner);
    outerRect.moveCenter(rect().center());
    innerRect.moveCenter(rect().center());

    if (isTextVisible()) {
        painter.save();
    }

    QPainterPath borderInAndOut;
    borderInAndOut.addEllipse(rect().center(), rect().width()/2*coefOuter, rect().height()/2*coefOuter);
    borderInAndOut.addEllipse(rect().center(), rect().width()/2*coefInner, rect().height()/2*coefInner);

    QPen borderPen(borderColor, penSize);
    painter.setPen(borderPen);
    painter.setBrush(grooveColor);
    painter.drawPath(borderInAndOut);

    if (value() > 0) {
        QPainterPath groovePath(rect().center());
        qreal converterAngle = 3.6*value();
        groovePath.arcTo(outerRect, startAngle, -converterAngle);
        groovePath.moveTo(rect().center());
        groovePath.arcTo(innerRect, startAngle, -converterAngle);
        groovePath = groovePath.simplified();
        painter.setPen(Qt::NoPen);
        painter.setBrush(chunkColor);
        painter.drawPath(groovePath);
    }

    if (!transparentCenter) {
        QPainterPath painterPathCenter;
        painterPathCenter.addEllipse(rect().center(), rect().width()/2*coefInner - penSize/2, rect().height()/2*coefInner - penSize/2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(Qt::white));
        painter.drawPath(painterPathCenter);
    }

    if (isTextVisible()) {
        painter.restore();
        QString val = QString::number(value());//.append("%");
        //QStyleOptionProgressBarV2 styleOption;
        //style()->drawControl(QStyle::CE_ProgressBarGroove, &styleOption, &painter, this);
        //style()->drawPrimitive(QStyle::PE_FrameStatusBar, &styleOption, &painter, this);
        style()->drawItemText(&painter, rect(), Qt::AlignCenter, palette(), true, val);
    }
}
