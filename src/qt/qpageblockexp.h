#ifndef QPAGEBLOCKEXP_H
#define QPAGEBLOCKEXP_H

#include "qstealthpage.h"
#include "qhoverbutton.h"
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>

class QPageBlockExp : public QStealthPage
{
    Q_OBJECT

public:
    explicit QPageBlockExp(QWidget *parent = 0);

private:
    QLabel              *lblBottom;

    QSpinBox            *spbFirst;
    QLineEdit           *edtSecond;

    QHoverButton        *btnJump;
    QHoverButton        *btnDecode;

    QLabel              *lbls[26];
    QLabel              *lblLabel2;

    QLabel              *lblContent1;
    QLabel              *lblContent2;

    QHoverButton        *btnIncAmount;
    QHoverButton        *btnDecAmount;
};

#endif // QPAGEBLOCKEXP_H
