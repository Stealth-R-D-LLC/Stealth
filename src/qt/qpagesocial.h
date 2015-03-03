#ifndef QPAGESOCIAL_H
#define QPAGESOCIAL_H

#include "qstealthpage.h"
#include "qhoverbutton.h"
#include <QLabel>
#include <QTextEdit>

#define SC_SOCIAL_BTN_COUNT         4

class QPageSocial : public QStealthPage
{
    Q_OBJECT
public:
    explicit QPageSocial(QWidget *parent = 0);

signals:

public slots:

private:
    QStealthBlueInfo    *txtBanner;
    QLabel              *lblSlide;
    QLabel              *lblLRArrow;
    QHoverButton        *btnLeft;
    QHoverButton        *btnRight;
    QLabel              *lblSocial;
    QHoverButton        *btn[SC_SOCIAL_BTN_COUNT];
};

#endif // QPAGESOCIAL_H
