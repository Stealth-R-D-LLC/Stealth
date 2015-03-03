#ifndef QPAGEOVERVIEW_H
#define QPAGEOVERVIEW_H

#include "qstealthpage.h"
#include "qhoverbutton.h"
#include "qstealthtableview.h"
#include <QLabel>
#include <QTextEdit>
#include "walletmodel.h"
#include "transactionfilterproxy.h"

#define SC_OVERVIEW_RECENT_LIST_COUNT           6

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

//class WalletModel;
class TxViewDelegate;
//class TransactionFilterProxy;

/** Overview ("home") page widget */
class QPageOverview : public QStealthPage
{
    Q_OBJECT
public:
    explicit QPageOverview(QWidget *parent = 0);
    void displayBannerInfo();

    void setModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);

signals:
    void transactionClicked(const QModelIndex &index);

public slots:
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);
    void setNumTransactions(int count);

private slots:
    void updateDisplayUnit();
    void handleTransactionClicked(const QModelIndex &index);

private:
    QStealthBlueInfo    *txtBanner;
    QSubTitle           *lblSubTitle;
    QStealthTableView   *table;

    WalletModel *model;
    bool                fOutOfSync;
    bool                fShowImmature;
    qint64 nCountOfTransactions;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;

    TxViewDelegate *txdelegate;
    TransactionFilterProxy *filter;
};

#endif // QPAGEOVERVIEW_H
