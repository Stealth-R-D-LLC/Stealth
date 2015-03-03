#ifndef QPAGEINCOMEEXPENSE_H
#define QPAGEINCOMEEXPENSE_H

#include "qstealthpage.h"
#include "qhoverbutton.h"
#include "qvalidatedlineedit.h"
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include "walletmodel.h"
#include "transactionfilterproxy.h"

class QPageIncomeExpense : public QStealthPage
{
    Q_OBJECT
public:
    explicit QPageIncomeExpense(QWidget *parent = 0);
    void displayBannerInfo();

    void setModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);

signals:

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);
    void setNumTransactions(int count);

private:
    QStealthBlueInfo    *txtBanner;
    QLabel              *lblLabel[5];
    QValidatedLineEdit  *edtInput[5];

    WalletModel *model;
    bool                fOutOfSync;
    bool                fShowImmature;
    qint64 nCountOfTransactions;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;
};

#endif // QPAGEINCOMEEXPENSE_H
