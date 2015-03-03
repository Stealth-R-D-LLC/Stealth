#include "qhoverbutton.h"

QHoverButton::QHoverButton(QWidget *parent, int id) :
    QPushButton(parent)
{
    this->id = id;
    this->deactivated = false;
    type = SC_HOVER_BUTTON_TYPE_NONE;
    lblFront = new QLabel(this);
    lblFront->setAlignment(Qt::AlignCenter);

    setFocusPolicy(Qt::NoFocus);

    connect(this, SIGNAL(clicked()), this, SLOT(slotClicked()));
}

void QHoverButton::setText(QString text)
{
    lblFront->setText(text);
}

void QHoverButton::setGeometry(QRect rect)
{
    QPushButton::setGeometry(rect);
    QRect newRect = rect;
    newRect.moveTo(0, 0);
    lblFront->setGeometry(newRect);
}

void QHoverButton::setGeometry(int x, int y, int w, int h)
{
    QPushButton::setGeometry(x, y, w, h);
    lblFront->setGeometry(0, 0, w, h);
}

void QHoverButton::enterEvent(QEvent *)
{
    setState(SC_BUTTON_STATE_HOVER);
    emit entered();
}

void QHoverButton::leaveEvent(QEvent *)
{
    setState(SC_BUTTON_STATE_DEFAULT);
    emit left();
}

void QHoverButton::setState(SC_BUTTON_STATE state)
{
    this->state = state;

    QString qss;

    qss = (state == SC_BUTTON_STATE_DEFAULT) ? qssBackDefault : qssBackHover;
    if (qss != "") this->setStyleSheet(qss);

    qss = (state == SC_BUTTON_STATE_DEFAULT) ? qssFrontDefault : qssFrontHover;
    if (qss != "") lblFront->setStyleSheet(qss);
}

void QHoverButton::setDeactivated(bool deactivate) {
    this->deactivated = deactivate;
}

void QHoverButton::setFrontStyleSheet(QString qssDefault, QString qssHover)
{
    if (qssHover == "") qssHover = qssDefault;

    this->qssFrontDefault = qssDefault;
    this->qssFrontHover = qssHover;
}

void QHoverButton::setBackStyleSheet(QString qssDefault, QString qssHover)
{
    if (qssHover == "" || qssDefault == "") return;
    if (qssHover == "") qssHover = qssDefault;

    this->qssBackDefault = qssDefault;
    this->qssBackHover = qssHover;
}

void QHoverButton::slotClicked()
{
    emit clicked(id);
}
