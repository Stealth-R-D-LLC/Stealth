#include "qpageoverview.h"
#include "common/qstealth.h"
#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QHeaderView>

#define DECORATION_SIZE 64
#define NUM_ITEMS 6

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(BitcoinUnits::BTC)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(value.canConvert<QColor>())
        {
            foreground = qvariant_cast<QColor>(value);
        }

        painter->setPen(foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
};


QPageOverview::QPageOverview(QWidget *parent) :
    QStealthPage(parent),
    nCountOfTransactions(0),
    currentBalance(0),
    currentStake(0),
    currentUnconfirmedBalance(0),
    currentImmatureBalance(0),
    txdelegate(new TxViewDelegate()),
    filter(0)
{
    txtBanner = new QStealthBlueInfo(STEALTH_PAGE_ID_OVERVIEW, this);

    fOutOfSync = true;
    fShowImmature = true;

    lblSubTitle = new QSubTitle(this);
    lblSubTitle->setFont(GUIUtil::stealthAppFont());
    lblSubTitle->setGeometry(0, 180, SC_PAGE_WIDTH, 50);

    QString qss = "background-color: #000; background-image: url(";
    qss += SC_ICON_PATH_ACTIVE;
    qss += "ico_transactions";
    qss += ".png); background-repeat: no-repeat; background-position: center;";
    lblSubTitle->lblIcon->setStyleSheet(qss);
    lblSubTitle->lblText->setText("RECENT TRANSACTIONS");

    table = new QStealthTableView(this);
    table->setGeometry(0, 230, SC_PAGE_WIDTH, SC_PAGE_HEIGHT - 230);
    table->hideHeader();

    connect(table->table, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

}

void QPageOverview::displayBannerInfo()
{
    if (model == NULL) return;
    if (model->getOptionsModel() == NULL) return;

    qint64 netWorth = currentBalance + currentImmatureBalance;
 
    int unit = model->getOptionsModel()->getDisplayUnit();

    txtBanner->setOverviewInfo(
        fOutOfSync,
        // the names of these values here and in the GUI are different
#if 1
        BitcoinUnits::formatWithUnit(unit, netWorth),
        BitcoinUnits::formatWithUnit(unit, currentBalance),
        BitcoinUnits::formatWithUnit(unit, currentStake),
        BitcoinUnits::formatWithUnit(unit, currentUnconfirmedBalance),
        BitcoinUnits::formatWithUnit(unit, currentImmatureBalance),
        QString().setNum((nCountOfTransactions > 0) ? nCountOfTransactions : 0));
#elif 0
        BitcoinUnits::formatWithUnit(unit, (1000 + 100)*1000000),
        BitcoinUnits::formatWithUnit(unit, 1000*1000000),
        BitcoinUnits::formatWithUnit(unit, 101*1000000),
        BitcoinUnits::formatWithUnit(unit, 0*1000000),
        BitcoinUnits::formatWithUnit(unit, 100*1000000),
        QString().setNum(100));
#else
        BitcoinUnits::formatWithUnit(unit, 888888888888888),
        BitcoinUnits::formatWithUnit(unit, 888888888888888),
        BitcoinUnits::formatWithUnit(unit, 888888888888888),
        BitcoinUnits::formatWithUnit(unit, 888888888888888),
        BitcoinUnits::formatWithUnit(unit, 888888888888888),
        QString().setNum(88888888));
#endif

}

void QPageOverview::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

void QPageOverview::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    fShowImmature = (immatureBalance != 0);
    displayBannerInfo();
}

void QPageOverview::setNumTransactions(int count)
{
    nCountOfTransactions = count;
    displayBannerInfo();
}

void QPageOverview::setModel(WalletModel *model)
{
    this->model = model;

    if (model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

//        ui->listTransactions->setModel(filter);
//        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);
        table->setModel(filter);
        table->table->horizontalHeader()->resizeSection(
                TransactionTableModel::Status, 40);
        table->table->horizontalHeader()->resizeSection(
                TransactionTableModel::Date, 120);
        table->table->horizontalHeader()->resizeSection(
                TransactionTableModel::Type, 160);
        table->table->horizontalHeader()->resizeSection(
                TransactionTableModel::ToAddress, 320);
#if QT_VERSION <0x050000
        table->table->horizontalHeader()->setResizeMode(
                TransactionTableModel::ToAddress, QHeaderView::Stretch);
#else
        table->table->horizontalHeader()->setSectionResizeMode(
                TransactionTableModel::ToAddress, QHeaderView::Stretch);
#endif
        table->table->horizontalHeader()->resizeSection(
                TransactionTableModel::Amount, 100);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));

        setNumTransactions(model->getNumTransactions());
        connect(model, SIGNAL(numTransactionsChanged(int)), this, SLOT(setNumTransactions(int)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
    }

    // update the display unit, to not use the default ("XST")
    updateDisplayUnit();
}

void QPageOverview::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, model->getStake(), currentUnconfirmedBalance, currentImmatureBalance);

        // Update txdelegate->unit with the current unit
//        txdelegate->unit = model->getOptionsModel()->getDisplayUnit();

//        ui->listTransactions->update();
    }
}

void QPageOverview::showOutOfSyncWarning(bool fShow)
{
    fOutOfSync = fShow;
//    ui->labelWalletStatus->setVisible(fShow);
//    ui->labelTransactionsStatus->setVisible(fShow);
    displayBannerInfo();
}
