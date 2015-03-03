#include "qpageaddrbook.h"
#include "addresstablemodel.h"
#include "optionsmodel.h"
#include "bitcoingui.h"
#include "editaddressdialog.h"
#include "csvmodelwriter.h"
#include "guiutil.h"

#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QClipboard>
#include <QMessageBox>
#include <QMenu>

#ifdef USE_QRCODE
#include "qrcodedialog.h"
#endif

QPageAddrBook::QPageAddrBook(Mode mode, Tabs tab, QWidget *parent) :
    QStealthPage(parent),
    model(0),
    optionsModel(0),
    mode(mode),
    tab(tab)
{
    table = new QStealthTableView(this);
    table->setGeometry(0, 130, SC_PAGE_WIDTH, SC_PAGE_HEIGHT - 210);
    createHeader();

    QString imageName[6] = {
        "bg_new_addr",
        "bg_copy_addr",
        "bg_show_qr",
        "bg_verify_msg",
        "bg_sign_msg",
        "bg_delete"
    };

    QString toolTipString[6] = {
        "Create a new address",
        "Copy the currently selected address to the system clipboard",
        "Show QR code",
        "Sign a message to prove you own a StealthCoin address",
        "Verify a message to ensure it was signed with a specified StealthCoin address",
        "Delete the currently selected address from the list"
    };

    for (int i = 0; i < 6; i++) {
        btns[i] = new QHoverButton(this, i);
        btns[i]->setGeometry(20 + (i < 4 ? i : i - 1) * 160, SC_PAGE_HEIGHT - 60, 140, 40);
        btns[i]->setFrontStyleSheet(
            QString("background-image: url(") + SC_BTN_PATH_DEFAULT + imageName[i] + ".png); background-position: center; background-repeat: no-repeat;",
            QString("background-image: url(") + SC_BTN_PATH_ACTIVE + imageName[i] + ".png); background-position: center; background-repeat: no-repeat;"
        );
        btns[i]->setState(SC_BUTTON_STATE_DEFAULT);
        btns[i]->setToolTip(toolTipString[i]);
        btns[i]->setStyleSheet(SC_QSS_TOOLTIP_DEFAULT);
    }

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    btns[BTN_ID_NEW_ADDR]->setIcon(QIcon());
    btns[BTN_ID_COPY_ADDR]->setIcon(QIcon());
    btns[BTN_ID_DELETE]->setIcon(QIcon());
#endif

#ifndef USE_QRCODE
    btns[BTN_ID_SHOW_QR]->setVisible(false);
#endif

    switch(mode)
    {
    case ForSending:
        connect(table->table, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(accept()));
        table->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->table->setFocus();
        break;
    case ForEditing:
//        ui->buttonBox->setVisible(false);
        break;
    }

    switch(tab)
    {
    case SendingTab:
        txtBanner = new QStealthBlueInfo(STEALTH_PAGE_ID_ADDRESS_BOOK, this);
        btns[BTN_ID_DELETE]->setVisible(true);
        btns[BTN_ID_SIGN_MSG]->setVisible(false);
        break;
    case ReceivingTab:
        txtBanner = new QStealthBlueInfo(STEALTH_PAGE_ID_RECEIVE_XST, this);
        btns[BTN_ID_DELETE]->setVisible(false);
        btns[BTN_ID_SIGN_MSG]->setVisible(true);
        break;
    }

    // Context menu actions
    QAction *copyLabelAction = new QAction(tr("Copy &Label"), this);
    QAction *copyAddressAction = new QAction(btns[BTN_ID_COPY_ADDR]->text(), this);
    QAction *editAction = new QAction(tr("&Edit"), this);
    QAction *showQRCodeAction = new QAction(btns[BTN_ID_SHOW_QR]->text(), this);
    QAction *signMessageAction = new QAction(btns[BTN_ID_SIGN_MSG]->text(), this);
    QAction *verifyMessageAction = new QAction(btns[BTN_ID_VERIFY_MSG]->text(), this);
    deleteAction = new QAction(btns[BTN_ID_DELETE]->text(), this);

    // Build context menu
    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(editAction);
    if(tab == SendingTab)
        contextMenu->addAction(deleteAction);
    contextMenu->addSeparator();
    contextMenu->addAction(showQRCodeAction);
    if(tab == ReceivingTab)
        contextMenu->addAction(signMessageAction);
    else if(tab == SendingTab)
        contextMenu->addAction(verifyMessageAction);

    // Connect signals for context menu actions
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(on_copyToClipboard_clicked()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(onCopyLabelAction()));
    connect(editAction, SIGNAL(triggered()), this, SLOT(onEditAction()));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(on_deleteButton_clicked()));
    connect(showQRCodeAction, SIGNAL(triggered()), this, SLOT(on_showQRCode_clicked()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(on_signMessage_clicked()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(on_verifyMessage_clicked()));

    connect(table->table, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    // Pass through accept action from button box
//    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(accept()));

    connect(btns[BTN_ID_NEW_ADDR], SIGNAL(clicked()), this, SLOT(on_newAddressButton_clicked()));
    connect(btns[BTN_ID_COPY_ADDR], SIGNAL(clicked()), this, SLOT(on_copyToClipboard_clicked()));
    connect(btns[BTN_ID_SHOW_QR], SIGNAL(clicked()), this, SLOT(on_showQRCode_clicked()));
    connect(btns[BTN_ID_VERIFY_MSG], SIGNAL(clicked()), this, SLOT(on_verifyMessage_clicked()));
    connect(btns[BTN_ID_SIGN_MSG], SIGNAL(clicked()), this, SLOT(on_signMessage_clicked()));
    connect(btns[BTN_ID_DELETE], SIGNAL(clicked()), this, SLOT(on_deleteButton_clicked()));
}

void QPageAddrBook::setModel(AddressTableModel *model)
{
    this->model = model;
    if(!model)
        return;

    proxyModel = new QSortFilterProxyModel(this);
    proxyModel->setSourceModel(model);
    proxyModel->setDynamicSortFilter(true);
    proxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    switch(tab)
    {
    case ReceivingTab:
        // Receive filter
        proxyModel->setFilterRole(AddressTableModel::TypeRole);
        proxyModel->setFilterFixedString(AddressTableModel::Receive);
        break;
    case SendingTab:
        // Send filter
        proxyModel->setFilterRole(AddressTableModel::TypeRole);
        proxyModel->setFilterFixedString(AddressTableModel::Send);
        break;
    }
    table->setModel(proxyModel);
    table->table->sortByColumn(0, Qt::AscendingOrder);

    // Set column widths
    table->table->horizontalHeader()->resizeSection(
            AddressTableModel::Label, 320);
#if QT_VERSION <0x050000
    table->table->horizontalHeader()->setResizeMode(
            AddressTableModel::Address, QHeaderView::Stretch);
#else
    table->table->horizontalHeader()->setSectionResizeMode(
            AddressTableModel::Address, QHeaderView::Stretch);
#endif

    connect(table->table->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(selectionChanged()));

    // Select row for newly created address
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(selectNewAddress(QModelIndex,int,int)));

    selectionChanged();
}

void QPageAddrBook::createHeader()
{
    // create table widget
    QStealthTableHeader *header = new QStealthTableHeader(table, 2);
    header->setStyleSheet("background-color: #999999;");

    QString slQss[2] = {
        QString() + "background-color: #4d4d4d; font-size: 13px; padding: 0px 10px; color: #fff;",
        QString() + "background-color: #999999; font-size: 13px; padding: 0px 10px; color: #fff;"
    };

    int woh[2] = {330, 495};

    QString toh[2] = {
        "LABEL",
        "ADDRESS"
    };

    for (int i = 0; i < 2; i++) {
        header->setFieldStyleSheet(i, slQss[i]);
        header->setFieldWidth(i, woh[i]);
        header->setFieldText(i, toh[i]);
        header->setFieldSortDirection(i, -1);
    }
    // set geometry
    header->setGeometry(0, 0, SC_PAGE_WIDTH, 50);
    header->update();
    table->setHeader(header);
}

void QPageAddrBook::setOptionsModel(OptionsModel *optionsModel)
{
    this->optionsModel = optionsModel;
}

void QPageAddrBook::on_copyToClipboard_clicked()
{
    GUIUtil::copyEntryData(table->table, AddressTableModel::Address);
}

void QPageAddrBook::onCopyLabelAction()
{
    GUIUtil::copyEntryData(table->table, AddressTableModel::Label);
}

void QPageAddrBook::onEditAction()
{
    if(!table->table->selectionModel())
        return;
    QModelIndexList indexes = table->table->selectionModel()->selectedRows();
    if(indexes.isEmpty())
        return;

    EditAddressDialog dlg(
            tab == SendingTab ?
            EditAddressDialog::EditSendingAddress :
            EditAddressDialog::EditReceivingAddress);
    dlg.setModel(model);
    QModelIndex origIndex = proxyModel->mapToSource(indexes.at(0));
    dlg.loadRow(origIndex.row());
    dlg.exec();
}

void QPageAddrBook::on_signMessage_clicked()
{
    QTableView *tableView = table->table;
    QModelIndexList indexes = tableView->selectionModel()->selectedRows(AddressTableModel::Address);
    QString addr;

    foreach (QModelIndex index, indexes)
    {
        QVariant address = index.data();
        addr = address.toString();
    }

    emit signMessage(addr);
}

void QPageAddrBook::on_verifyMessage_clicked()
{
    QTableView *tableView = table->table;
    QModelIndexList indexes = tableView->selectionModel()->selectedRows(AddressTableModel::Address);
    QString addr;

    foreach (QModelIndex index, indexes)
    {
        QVariant address = index.data();
        addr = address.toString();
    }

    emit verifyMessage(addr);
}

void QPageAddrBook::on_newAddressButton_clicked()
{
    if(!model)
        return;
    EditAddressDialog dlg(
            tab == SendingTab ?
            EditAddressDialog::NewSendingAddress :
            EditAddressDialog::NewReceivingAddress, this);
    dlg.setModel(model);
    if(dlg.exec())
    {
        newAddressToSelect = dlg.getAddress();
    }
}

void QPageAddrBook::on_deleteButton_clicked()
{
    QTableView *tableView = table->table;
    if(!tableView->selectionModel())
        return;
    QModelIndexList indexes = tableView->selectionModel()->selectedRows();
    if(!indexes.isEmpty())
    {
        tableView->model()->removeRow(indexes.at(0).row());
    }
}

void QPageAddrBook::selectionChanged()
{
    // Set button states based on selected tab and selection
    QTableView *tableView = table->table;
    if(!tableView->selectionModel())
        return;

    if(tableView->selectionModel()->hasSelection())
    {
        switch(tab)
        {
        case SendingTab:
            // In sending tab, allow deletion of selection
            btns[BTN_ID_DELETE]->setEnabled(true);
            btns[BTN_ID_DELETE]->setVisible(true);
            deleteAction->setEnabled(true);
            btns[BTN_ID_SIGN_MSG]->setEnabled(false);
            btns[BTN_ID_SIGN_MSG]->setVisible(false);
            btns[BTN_ID_VERIFY_MSG]->setEnabled(true);
            btns[BTN_ID_VERIFY_MSG]->setVisible(true);
            break;
        case ReceivingTab:
            // Deleting receiving addresses, however, is not allowed
            btns[BTN_ID_DELETE]->setEnabled(false);
            btns[BTN_ID_DELETE]->setVisible(false);
            deleteAction->setEnabled(false);
            btns[BTN_ID_SIGN_MSG]->setEnabled(true);
            btns[BTN_ID_SIGN_MSG]->setVisible(true);
            btns[BTN_ID_VERIFY_MSG]->setEnabled(false);
            btns[BTN_ID_VERIFY_MSG]->setVisible(false);
            break;
        }
        btns[BTN_ID_COPY_ADDR]->setEnabled(true);
        btns[BTN_ID_SHOW_QR]->setEnabled(true);
    }
    else
    {
        btns[BTN_ID_DELETE]->setEnabled(false);
        btns[BTN_ID_SHOW_QR]->setEnabled(false);
        btns[BTN_ID_COPY_ADDR]->setEnabled(false);
        btns[BTN_ID_SIGN_MSG]->setEnabled(false);
        btns[BTN_ID_VERIFY_MSG]->setEnabled(false);
    }
}

//void QPageAddrBook::done(int retval)
//{
//    QTableView *tableView = table->table;
//    if(!tableView->selectionModel() || !tableView->model())
//        return;
//    // When this is a tab/widget and not a model dialog, ignore "done"
//    if(mode == ForEditing)
//        return;

//    // Figure out which address was selected, and return it
//    QModelIndexList indexes = tableView->selectionModel()->selectedRows(AddressTableModel::Address);

//    foreach (QModelIndex index, indexes)
//    {
//        QVariant address = tableView->model()->data(index);
//        returnValue = address.toString();
//    }

//    if(returnValue.isEmpty())
//    {
//        // If no address entry selected, return rejected
//        retval = Rejected;
//    }

//    QDialog::done(retval);
//}

void QPageAddrBook::exportClicked()
{
    // CSV is currently the only supported format
    QString filename = GUIUtil::getSaveFileName(
            this,
            tr("Export Address Book Data"), QString(),
            tr("Comma separated file (*.csv)"));

    if (filename.isNull()) return;

    CSVModelWriter writer(filename);

    // name, column, role
    writer.setModel(proxyModel);
    writer.addColumn("Label", AddressTableModel::Label, Qt::EditRole);
    writer.addColumn("Address", AddressTableModel::Address, Qt::EditRole);

    if (!writer.write())
    {
        QMessageBox::critical(this, tr("Error exporting"), tr("Could not write to file %1.").arg(filename),
                              QMessageBox::Abort, QMessageBox::Abort);
    }
}

void QPageAddrBook::on_showQRCode_clicked()
{
#ifdef USE_QRCODE
    QTableView *tableView = table->table;
    QModelIndexList indexes = tableView->selectionModel()->selectedRows(AddressTableModel::Address);

    foreach (QModelIndex index, indexes)
    {
        QString address = index.data().toString(), label = index.sibling(index.row(), 0).data(Qt::EditRole).toString();

        QRCodeDialog *dialog = new QRCodeDialog(address, label, tab == ReceivingTab, this);
        if(optionsModel)
            dialog->setModel(optionsModel);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }
#endif
}

void QPageAddrBook::contextualMenu(const QPoint &point)
{
    QModelIndex index = table->table->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void QPageAddrBook::selectNewAddress(const QModelIndex &parent, int begin, int end)
{
    QModelIndex idx = proxyModel->mapFromSource(model->index(begin, AddressTableModel::Address, parent));
    if(idx.isValid() && (idx.data(Qt::EditRole).toString() == newAddressToSelect))
    {
        // Select row of newly created address, once
        table->table->setFocus();
        table->table->selectRow(idx.row());
        newAddressToSelect.clear();
    }
}
