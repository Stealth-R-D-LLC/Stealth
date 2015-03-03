#ifndef QPAGESENDXST_H
#define QPAGESENDXST_H

#include "qstealthpage.h"
#include "qhoverbutton.h"
#include "sendcoinsdialog.h"
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>

class QPageSendXST : public QStealthPage
{
    Q_OBJECT
public:
    explicit QPageSendXST(QWidget *parent = 0);

    void setModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);

public:
    SendCoinsDialog *sendCoinsPage;
};

#endif // QPAGESENDXST_H
