#include "qpageincomeexpense.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "guiutil.h"
#include "guiconstants.h"


QPageIncomeExpense::QPageIncomeExpense(QWidget *parent) :
    QStealthPage(parent),
    nCountOfTransactions(0),
    currentBalance(0),
    currentStake(0),
    currentUnconfirmedBalance(0),
    currentImmatureBalance(0),
    fOutOfSync(1)
{
    txtBanner = new QStealthBlueInfo(STEALTH_PAGE_ID_OVERVIEW, this);

    for (int i = 0; i < 5; i++) {
        lblLabel[i] = new QLabel(this);
        lblLabel[i]->setGeometry(40, 220 + i * 60, 350, 40);
        lblLabel[i]->setStyleSheet("background-color: #999; border: none; color: white; padding-left: 20px;");
        edtInput[i] = new QValidatedLineEdit(this);
        edtInput[i]->setGeometry(390, 220 + i * 60, 395, 40);
        edtInput[i]->makeTriAngle();

//        edtInput[i]->setStyleSheet("background-color: #fff; padding: 0px 20px; border: none;");
//        edtInput[i]->setAlignment(Qt::AlignRight);
    }
    lblLabel[0]->setText("NUMBER OF TRANSACTIONS");
    lblLabel[1]->setText("INCOMING TRANSACTIONS");
    lblLabel[2]->setText("XST RECEIVED");
    lblLabel[3]->setText("OUTGOING TRANSACTIONS");
    lblLabel[4]->setText("XST SENT");
}


void QPageIncomeExpense::displayBannerInfo()
{
    if (model == NULL) return;
    if (model->getOptionsModel() == NULL) return;

    qint64 spendable = currentBalance - currentStake;
    int unit = model->getOptionsModel()->getDisplayUnit();

    txtBanner->setOverviewInfo(
        fOutOfSync,
        BitcoinUnits::formatWithUnit(unit, currentBalance),
        BitcoinUnits::formatWithUnit(unit, spendable),
        BitcoinUnits::formatWithUnit(unit, currentStake),
        BitcoinUnits::formatWithUnit(unit, currentUnconfirmedBalance),
        BitcoinUnits::formatWithUnit(unit, currentImmatureBalance),
        QString().setNum((nCountOfTransactions > 0) ? nCountOfTransactions : 0));
}

void QPageIncomeExpense::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    fShowImmature = immatureBalance != 0;
    displayBannerInfo();
}

void QPageIncomeExpense::setNumTransactions(int count)
{
    nCountOfTransactions = count;
    displayBannerInfo();
}

void QPageIncomeExpense::setModel(WalletModel *model)
{
    this->model = model;

    if (model && model->getOptionsModel())
    {
        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        setNumTransactions(model->getNumTransactions());
        connect(model, SIGNAL(numTransactionsChanged(int)), this, SLOT(setNumTransactions(int)));
    }
}

void QPageIncomeExpense::showOutOfSyncWarning(bool fShow)
{
    fOutOfSync = fShow;
    displayBannerInfo();
}
