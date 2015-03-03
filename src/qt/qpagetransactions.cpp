#include "qpagetransactions.h"

#include "transactionfilterproxy.h"
#include "transactionrecord.h"
#include "walletmodel.h"
#include "addresstablemodel.h"
#include "transactiontablemodel.h"
#include "bitcoinunits.h"
#include "csvmodelwriter.h"
#include "transactiondescdialog.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "guiutil.h"

#include <QScrollBar>
#include <QComboBox>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QTableView>
#include <QHeaderView>
#include <QPushButton>
#include <QMessageBox>
#include <QPoint>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QDateTimeEdit>

#include "qstealthpage.h"

QPageTransactions::QPageTransactions(QWidget *parent) :
    QStealthPage(parent), model(0)
{
    // Build filter row
    setContentsMargins(0,0,0,0);

//    QHBoxLayout *hlayout = new QHBoxLayout();
//    hlayout->setContentsMargins(0,0,0,0);
//#ifdef Q_OS_MAC
//    hlayout->setSpacing(5);
//    hlayout->addSpacing(26);
//#else
//    hlayout->setSpacing(0);
//    hlayout->addSpacing(23);
//#endif

    transactionProxyModel = NULL;
    table = NULL;

    dateWidget = new QComboBox(this);
#ifdef Q_OS_MAC
    dateWidget->setFixedWidth(121);
#else
    dateWidget->setFixedWidth(120);
#endif
    dateWidget->addItem(tr("All"), All);
    dateWidget->addItem(tr("Today"), Today);
    dateWidget->addItem(tr("This week"), ThisWeek);
    dateWidget->addItem(tr("This month"), ThisMonth);
    dateWidget->addItem(tr("Last month"), LastMonth);
    dateWidget->addItem(tr("This year"), ThisYear);
    dateWidget->addItem(tr("Range..."), Range);
//    hlayout->addWidget(dateWidget);

    typeWidget = new QComboBox(this);
#ifdef Q_OS_MAC
    typeWidget->setFixedWidth(121);
#else
    typeWidget->setFixedWidth(120);
#endif

    typeWidget->addItem(tr("All"), TransactionFilterProxy::ALL_TYPES);
    typeWidget->addItem(tr("Received with"), TransactionFilterProxy::TYPE(TransactionRecord::RecvWithAddress) |
                                        TransactionFilterProxy::TYPE(TransactionRecord::RecvFromOther));
    typeWidget->addItem(tr("Sent to"), TransactionFilterProxy::TYPE(TransactionRecord::SendToAddress) |
                                  TransactionFilterProxy::TYPE(TransactionRecord::SendToOther));
    typeWidget->addItem(tr("To yourself"), TransactionFilterProxy::TYPE(TransactionRecord::SendToSelf));
    typeWidget->addItem(tr("Minted"), TransactionFilterProxy::TYPE(TransactionRecord::StakeMint));
    typeWidget->addItem(tr("Mined"), TransactionFilterProxy::TYPE(TransactionRecord::Generated));
    typeWidget->addItem(tr("Other"), TransactionFilterProxy::TYPE(TransactionRecord::Other));

//    hlayout->addWidget(typeWidget);

    addressWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    /* Do not move this to the XML file, Qt before 4.7 will choke on it */
    addressWidget->setPlaceholderText(tr("Enter address or label to search"));
#endif
//    hlayout->addWidget(addressWidget);

    amountWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    /* Do not move this to the XML file, Qt before 4.7 will choke on it */
    amountWidget->setPlaceholderText(tr("Min amount"));
#endif
//#ifdef Q_OS_MAC
//    amountWidget->setFixedWidth(97);
//#else
//    amountWidget->setFixedWidth(100);
//#endif
    amountWidget->setValidator(new QDoubleValidator(0, 1e20, 8, this));
//    hlayout->addWidget(amountWidget);

//    QVBoxLayout *vlayout = new QVBoxLayout(this);
//    vlayout->setContentsMargins(0,0,0,0);
//    vlayout->setSpacing(0);

    QString qss = "";
    qss += "QComboBox { background-color: #4c4c4c; selection-background-color: #3cabe1; border: none; color: #fff; padding: 0px 10px; font-size: 14px; }";
    qss += "QComboBox::drop-down { border: none; background-image: url(':/resources/res/images/bg_combo_down.png'); background-position: center; background-repeat: no-repeat; }";
    qss += "QComboBox QAbstractItemView { border: none; color: #fff; background-color: #4c4c4c; selection-color: white; selection-background-color: #3cabe1; padding: 0px 5px; }";
    qss += "";

    dateWidget->setGeometry(50, 10, 115, 30);
    dateWidget->setStyleSheet(qss);
    typeWidget->setGeometry(175, 10, 115, 30);
    typeWidget->setStyleSheet(qss);

    addressWidget->setGeometry(300, 10, 350, 30);
    addressWidget->setStyleSheet("background-color: white; padding: 0px 10px; border: none; font-size: 12px;");
    addressWidget->setPlaceholderText("Enter address or label to search");

    amountWidget->setGeometry(660, 10, 120, 30);
    amountWidget->setStyleSheet("background-color: white; padding: 0px 10px; border: none; font-size: 12px;");
    amountWidget->setPlaceholderText("Min amount");

    createDateRangeWidget();
    dateRangeWidget->setGeometry(40, 50, 500, 30);

    QStealthTableView *view = new QStealthTableView(this);

//    vlayout->addLayout(hlayout);
//    vlayout->addWidget(createDateRangeWidget());
//    vlayout->addWidget(view);
//    vlayout->setSpacing(0);
//    int width = view->verticalScrollBar()->sizeHint().width();
    // Cover scroll bar width with spacing
//#ifdef Q_OS_MAC
//    hlayout->addSpacing(width+2);
//#else
//    hlayout->addSpacing(width);
//#endif
    // Always show scroll bar
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    view->table->setTabKeyNavigation(false);
    view->table->setContextMenuPolicy(Qt::CustomContextMenu);

    table = view;
    table->setGeometry(0, 50, SC_PAGE_WIDTH, SC_PAGE_HEIGHT - 50);

    // Actions
    QAction *copyAddressAction = new QAction(tr("Copy address"), this);
    QAction *copyLabelAction = new QAction(tr("Copy label"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);
    QAction *copyTxIDAction = new QAction(tr("Copy transaction ID"), this);
    QAction *editLabelAction = new QAction(tr("Edit label"), this);
    QAction *showDetailsAction = new QAction(tr("Show transaction details"), this);

    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTxIDAction);
    contextMenu->addAction(editLabelAction);
    contextMenu->addAction(showDetailsAction);

    // Connect actions
    connect(dateWidget, SIGNAL(activated(int)), this, SLOT(chooseDate(int)));
    connect(typeWidget, SIGNAL(activated(int)), this, SLOT(chooseType(int)));
    connect(addressWidget, SIGNAL(textChanged(QString)), this, SLOT(changedPrefix(QString)));
    connect(amountWidget, SIGNAL(textChanged(QString)), this, SLOT(changedAmount(QString)));

    connect(view->table, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(showDetails()));
    connect(view, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(copyAddress()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(copyLabel()));
    connect(copyAmountAction, SIGNAL(triggered()), this, SLOT(copyAmount()));
    connect(copyTxIDAction, SIGNAL(triggered()), this, SLOT(copyTxID()));
    connect(editLabelAction, SIGNAL(triggered()), this, SLOT(editLabel()));
    connect(showDetailsAction, SIGNAL(triggered()), this, SLOT(showDetails()));

    createHeader();
}

void QPageTransactions::setModel(WalletModel *model)
{
    this->model = model;
    if(model)
    {
        transactionProxyModel = new TransactionFilterProxy(this);
        transactionProxyModel->setSourceModel(model->getTransactionTableModel());
        transactionProxyModel->setDynamicSortFilter(true);
        transactionProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        transactionProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

        transactionProxyModel->setSortRole(Qt::EditRole);

        table->setModel(transactionProxyModel);
        //transactionView->setAlternatingRowColors(true);
        //transactionView->setSelectionBehavior(QAbstractItemView::SelectRows);
        //transactionView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        //transactionView->setSortingEnabled(true);
        //transactionView->verticalHeader()->hide();

        // transactionView->sortByColumn(TransactionTableModel::Date, Qt::DescendingOrder);


        table->table->sortByColumn(TransactionTableModel::Date,
                                   Qt::DescendingOrder);

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
    }
}

void QPageTransactions::chooseDate(int idx)
{
    if(!transactionProxyModel)
        return;
    QDate current = QDate::currentDate();
    dateRangeWidget->setVisible(false);
    resizeTable();
    switch(dateWidget->itemData(idx).toInt())
    {
    case All:
        transactionProxyModel->setDateRange(
                TransactionFilterProxy::MIN_DATE,
                TransactionFilterProxy::MAX_DATE);
        break;
    case Today:
        transactionProxyModel->setDateRange(
                QDateTime(current),
                TransactionFilterProxy::MAX_DATE);
        break;
    case ThisWeek: {
        // Find last Monday
        QDate startOfWeek = current.addDays(-(current.dayOfWeek()-1));
        transactionProxyModel->setDateRange(
                QDateTime(startOfWeek),
                TransactionFilterProxy::MAX_DATE);

        } break;
    case ThisMonth:
        transactionProxyModel->setDateRange(
                QDateTime(QDate(current.year(), current.month(), 1)),
                TransactionFilterProxy::MAX_DATE);
        break;
    case LastMonth:
        transactionProxyModel->setDateRange(
                QDateTime(QDate(current.year(), current.month()-1, 1)),
                QDateTime(QDate(current.year(), current.month(), 1)));
        break;
    case ThisYear:
        transactionProxyModel->setDateRange(
                QDateTime(QDate(current.year(), 1, 1)),
                TransactionFilterProxy::MAX_DATE);
        break;
    case Range:
        dateRangeWidget->setVisible(true);
        resizeTable();
        dateRangeChanged();
        break;
    }

    if (table) table->update();
}

void QPageTransactions::chooseType(int idx)
{
    if(!transactionProxyModel)
        return;
    transactionProxyModel->setTypeFilter(
        typeWidget->itemData(idx).toInt());

    if (table) table->update();
}

void QPageTransactions::changedPrefix(const QString &prefix)
{
    if(!transactionProxyModel)
        return;
    transactionProxyModel->setAddressPrefix(prefix);

    if (table) table->update();
}

void QPageTransactions::changedAmount(const QString &amount)
{
    if(!transactionProxyModel)
        return;
    qint64 amount_parsed = 0;
    if(BitcoinUnits::parse(model->getOptionsModel()->getDisplayUnit(), amount, &amount_parsed))
    {
        transactionProxyModel->setMinAmount(amount_parsed);
    }
    else
    {
        transactionProxyModel->setMinAmount(0);
    }

    if (table) table->update();
}

void QPageTransactions::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Transaction Data"), QString(),
            tr("Comma separated file (*.csv)"));

    if (filename.isNull()) return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(transactionProxyModel);
    writer.addColumn(tr("Confirmed"), 0, TransactionTableModel::ConfirmedRole);
    writer.addColumn(tr("Date"), 0, TransactionTableModel::DateRole);
    writer.addColumn(tr("Type"), TransactionTableModel::Type, Qt::EditRole);
    writer.addColumn(tr("Label"), 0, TransactionTableModel::LabelRole);
    writer.addColumn(tr("Address"), 0, TransactionTableModel::AddressRole);
    writer.addColumn(tr("Amount"), 0, TransactionTableModel::FormattedAmountRole);
    writer.addColumn(tr("ID"), 0, TransactionTableModel::TxIDRole);

    if(!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}

void QPageTransactions::contextualMenu(const QPoint &point)
{
    QModelIndex index = table->table->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void QPageTransactions::copyAddress()
{
    GUIUtil::copyEntryData(table->table, 0, TransactionTableModel::AddressRole);
}

void QPageTransactions::copyLabel()
{
    GUIUtil::copyEntryData(table->table, 0, TransactionTableModel::LabelRole);
}

void QPageTransactions::copyAmount()
{
    GUIUtil::copyEntryData(table->table, 0, TransactionTableModel::FormattedAmountRole);
}
void QPageTransactions::copyTxID()
{
    GUIUtil::copyEntryData(table->table, 0, TransactionTableModel::TxIDRole);
}
void QPageTransactions::editLabel()
{
    if(!table->table->selectionModel() ||!model)
        return;
    QModelIndexList selection = table->table->selectionModel()->selectedRows();
    if(!selection.isEmpty())
    {
        AddressTableModel *addressBook = model->getAddressTableModel();
        if(!addressBook)
            return;
        QString address = selection.at(0).data(TransactionTableModel::AddressRole).toString();
        if(address.isEmpty())
        {
            // If this transaction has no associated address, exit
            return;
        }
        // Is address in address book? Address book can miss address when a transaction is
        // sent from outside the UI.
        int idx = addressBook->lookupAddress(address);
        if(idx != -1)
        {
            // Edit sending / receiving address
            QModelIndex modelIdx = addressBook->index(idx, 0, QModelIndex());
            // Determine type of address, launch appropriate editor dialog type
            QString type = modelIdx.data(AddressTableModel::TypeRole).toString();

            EditAddressDialog dlg(type==AddressTableModel::Receive
                                         ? EditAddressDialog::EditReceivingAddress
                                         : EditAddressDialog::EditSendingAddress,
                                  this);
            dlg.setModel(addressBook);
            dlg.loadRow(idx);
            dlg.exec();
        }
        else
        {
            // Add sending address
            EditAddressDialog dlg(EditAddressDialog::NewSendingAddress,
                                  this);
            dlg.setModel(addressBook);
            dlg.setAddress(address);
            dlg.exec();
        }
    }
}

void QPageTransactions::showDetails()
{
    if(!table->table->selectionModel())
        return;
    QModelIndexList selection = table->table->selectionModel()->selectedRows();
    if(!selection.isEmpty())
    {
        TransactionDescDialog dlg(selection.at(0));
        dlg.exec();
    }
}

QWidget *QPageTransactions::createDateRangeWidget()
{
    dateRangeWidget = new QFrame(this);
//    dateRangeWidget->setFrameStyle(QFrame::Panel | QFrame::Raised);
//    dateRangeWidget->setContentsMargins(0, 0, 0, 0);
//    QHBoxLayout *layout = new QHBoxLayout(dateRangeWidget);
//    layout->setContentsMargins(0, 0, 0, 0);
//    layout->addSpacing(23);
//    layout->addWidget(new QLabel(tr("Range:")));
    QLabel *lblFrom = new QLabel(tr("Range"), dateRangeWidget);
    QLabel *lblTo = new QLabel(tr("To"), dateRangeWidget);

    dateFrom = new QDateTimeEdit(dateRangeWidget);
    dateFrom->setDisplayFormat("dd/MM/yy");
    dateFrom->setCalendarPopup(true);
    dateFrom->setMinimumWidth(100);
    dateFrom->setDate(QDate::currentDate().addDays(-7));
//    layout->addWidget(dateFrom);
//    layout->addWidget(new QLabel(tr("To")));

    dateTo = new QDateTimeEdit(dateRangeWidget);
    dateTo->setDisplayFormat("dd/MM/yy");
    dateTo->setCalendarPopup(true);
    dateTo->setMinimumWidth(100);
    dateTo->setDate(QDate::currentDate());
//    layout->addWidget(dateTo);
//    layout->addStretch();

    lblFrom->setGeometry(0, 0, 90, 30);
    dateFrom->setGeometry(90, 0, 150, 30);
    lblTo->setGeometry(240, 0, 90, 30);
    dateTo->setGeometry(330, 0, 150, 30);

    // Hide by default
    dateRangeWidget->setVisible(false);
    resizeTable();

    // Notify on change
    connect(dateFrom, SIGNAL(dateChanged(QDate)), this, SLOT(dateRangeChanged()));
    connect(dateTo, SIGNAL(dateChanged(QDate)), this, SLOT(dateRangeChanged()));

    dateRangeWidget->setStyleSheet("QFrame { border: none; }");
    lblFrom->setStyleSheet("QLabel { background-color: #4d4d4d; color: #fff; padding-left: 15px; }");
    lblTo->setStyleSheet("QLabel { background-color: #4d4d4d; color: #fff; padding-left: 15px; }");
    QString qss = "";
    qss += "QDateTimeEdit { border: none; padding-left: 15px; }";
    qss += "QDateTimeEdit::drop-down { width: 50px; border: none; background-image: url(':/resources/res/images/bg_date_spin.png'); background-position: center; background-repeat: no-repeat; }";
    qss += "";
    dateFrom->setStyleSheet(qss);
    dateTo->setStyleSheet(qss);

    return dateRangeWidget;
}

void QPageTransactions::dateRangeChanged()
{
    if(!transactionProxyModel)
        return;
    transactionProxyModel->setDateRange(
            QDateTime(dateFrom->date()),
            QDateTime(dateTo->date()).addDays(1));

    if (table) table->update();
}

void QPageTransactions::focusTransaction(const QModelIndex &idx)
{
    if(!transactionProxyModel)
        return;
    QModelIndex targetIdx = transactionProxyModel->mapFromSource(idx);
    table->table->scrollTo(targetIdx);
    table->table->setCurrentIndex(targetIdx);
    table->table->setFocus();
}

void QPageTransactions::createHeader()
{
    QStealthTableHeader *header = new QStealthTableHeader(table, 5);

    int woh[5] = {30, 150, 150, 300, 165};

    QString toh[5] = {
        "",
        "DATE",
        "TYPE",
        "ADDRESS",
        "AMOUNT"
    };

    for (int i = 0; i < 5; i++) {
        header->setFieldStyleSheet(i, "font-size: 13px; color: #fff;");
        header->setFieldWidth(i, woh[i]);
        header->setFieldText(i, toh[i]);
        header->setFieldSortDirection(i, -1);
    }
    header->setFieldEnable(0, false);

    // set geometry
    header->setGeometry(0, 0, SC_PAGE_WIDTH, 50);
    header->update();
    table->setHeader(header);
}

void QPageTransactions::resizeTable()
{
    if (dateRangeWidget == NULL) return;
    if (table == NULL) return;

    if (dateRangeWidget->isVisible()) {
        table->setGeometry(0, 90, SC_PAGE_WIDTH, SC_PAGE_HEIGHT - 90);
    }
    else {
        table->setGeometry(0, 50, SC_PAGE_WIDTH, SC_PAGE_HEIGHT - 50);
    }
    table->update();
}
