#include "sendcoinsdialog.h"
#include "common/qstealth.h"
#include "ui_sendcoinsdialog.h"
#include "init.h"
#include "walletmodel.h"
#include "addresstablemodel.h"
#include "addressbookpage.h"
#include "bitcoinunits.h"
#include "addressbookpage.h"
#include "optionsmodel.h"
#include "sendcoinsentry.h"
#include "guiutil.h"
#include "askpassphrasedialog.h"
#include "coincontrol.h"
#include "coincontroldialog.h"

#include <QMessageBox>
#include <QLocale>
#include <QTextDocument>
#include <QScrollBar>
#include <QClipboard>


extern bool fWalletUnlockMintOnly;

SendCoinsDialog::SendCoinsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SendCoinsDialog),
    model(0),
    fOutOfSync(1),
    fShowImmature(1),
    nCountOfTransactions(0),
    currentBalance(0),
    currentStake(0),
    currentUnconfirmedBalance(0),
    currentImmatureBalance(0)

{
    ui->setupUi(this);

#ifdef Q_OS_MAC // Icons on push buttons are very uncommon on Mac
    ui->addButton->setIcon(QIcon());
    ui->clearButton->setIcon(QIcon());
    ui->sendButton->setIcon(QIcon());
#endif

#if QT_VERSION >= 0x040700
     /* Do not move this to the XML file, Qt before 4.7 will choke on it */
     ui->lineEditCoinControlChange->setPlaceholderText(tr("Enter a StealthCoin address (e.g. SGVQhkwomS9tEDhwakwZC9aVqFtzEF1ZVG)"));
#endif

    addEntry();

    connect(ui->addButton, SIGNAL(clicked()), this, SLOT(addEntry()));
    connect(ui->clearButton, SIGNAL(clicked()), this, SLOT(clear()));
	
        // Coin Control
     ui->lineEditCoinControlChange->setFont(GUIUtil::bitcoinAddressFont());
     connect(ui->pushButtonCoinControl, SIGNAL(clicked()), this, SLOT(coinControlButtonClicked()));
     connect(ui->checkBoxCoinControlChange, SIGNAL(stateChanged(int)), this, SLOT(coinControlChangeChecked(int)));
     connect(ui->lineEditCoinControlChange, SIGNAL(textEdited(const QString &)), this, SLOT(coinControlChangeEdited(const QString &)));
 
		// Coin Control: clipboard actions
     QAction *clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
     QAction *clipboardAmountAction = new QAction(tr("Copy amount"), this);
     QAction *clipboardFeeAction = new QAction(tr("Copy fee"), this);
     QAction *clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
     QAction *clipboardBytesAction = new QAction(tr("Copy bytes"), this);
     QAction *clipboardPriorityAction = new QAction(tr("Copy priority"), this);
     QAction *clipboardLowOutputAction = new QAction(tr("Copy low output"), this);
     QAction *clipboardChangeAction = new QAction(tr("Copy change"), this);
     connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardQuantity()));
     connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAmount()));
     connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardFee()));
     connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardAfterFee()));
     connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardBytes()));
     connect(clipboardPriorityAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardPriority()));
     connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardLowOutput()));
     connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(coinControlClipboardChange()));
     ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
     ui->labelCoinControlAmount->addAction(clipboardAmountAction);
     ui->labelCoinControlFee->addAction(clipboardFeeAction);
     ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
     ui->labelCoinControlBytes->addAction(clipboardBytesAction);
     ui->labelCoinControlPriority->addAction(clipboardPriorityAction);
     ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
     ui->labelCoinControlChange->addAction(clipboardChangeAction);
 
     fNewRecipientAllowed = true;

     SC_sizeFontLabel(ui->labelBalance);
     ui->labelBalance->setMinimumSize(0, 20);


     customizeUI();
     displayBannerInfo();
}

SendCoinsDialog::~SendCoinsDialog()
{
    delete ui;
}

void SendCoinsDialog::setModel(WalletModel *model)
{
    this->model = model;

    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            entry->setModel(model);
        }
    }
    if(model && model->getOptionsModel())
    {
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64)));
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        setNumTransactions(model->getNumTransactions());
        connect(model, SIGNAL(numTransactionsChanged(int)), this, SLOT(setNumTransactions(int)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        updateDisplayUnit();
		
        // Coin Control
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(coinControlUpdateLabels()));
        connect(model->getOptionsModel(), SIGNAL(coinControlFeaturesChanged(bool)), this, SLOT(coinControlFeatureChanged(bool)));
        connect(model->getOptionsModel(), SIGNAL(transactionFeeChanged(qint64)), this, SLOT(coinControlUpdateLabels()));
        ui->frameCoinControl->setVisible(model->getOptionsModel()->getCoinControlFeatures());
        ui->frmBanner->setVisible(!(model->getOptionsModel()->getCoinControlFeatures()));
        coinControlUpdateLabels();
    }

    displayBannerInfo();
}

void SendCoinsDialog::showOutOfSyncWarning(bool fShow)
{
    fOutOfSync = fShow;
    displayBannerInfo();
}

void SendCoinsDialog::on_sendButton_clicked()
{
    bool mintonly = fWalletUnlockMintOnly;

    QList<SendCoinsRecipient> recipients;
    bool valid = true;

    if(!model)
        return;

    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            if(entry->validate())
            {
                recipients.append(entry->getValue());
            }
            else
            {
                valid = false;
            }
        }
    }

    if(!valid || recipients.isEmpty())
    {
        return;
    }

    // Format confirmation message
    QStringList formatted;
    foreach(const SendCoinsRecipient &rcp, recipients)
    {
#if QT_VERSION < 0x050000
        formatted.append(tr("<b>%1</b> to %2 (%3)").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, rcp.amount), Qt::escape(rcp.label), rcp.address));
#else
        formatted.append(tr("<b>%1</b> to %2 (%3)").arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, rcp.amount), rcp.label.toHtmlEscaped(), rcp.address));
#endif
    }

    fNewRecipientAllowed = false;

    QMessageBox::StandardButton retval = QMessageBox::question(this, tr("Confirm send coins"),
                          tr("Are you sure you want to send %1?").arg(formatted.join(tr(" and "))),
          QMessageBox::Yes|QMessageBox::Cancel,
          QMessageBox::Cancel);

    if(retval != QMessageBox::Yes)
    {
        fNewRecipientAllowed = true;
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        fNewRecipientAllowed = true;
        return;
    }

    WalletModel::SendCoinsReturn sendstatus;

    if (!model->getOptionsModel() || !model->getOptionsModel()->getCoinControlFeatures())
        sendstatus = model->sendCoins(recipients);
    else
        sendstatus = model->sendCoins(recipients, CoinControlDialog::coinControl);

    switch(sendstatus.status)
    {
    case WalletModel::InvalidAddress:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The recipient address is not valid, please recheck."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::InvalidAmount:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The amount to pay must be larger than 0."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The amount exceeds your balance."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("The total exceeds your balance when the %1 transaction fee is included.").
            arg(BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, sendstatus.fee)),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::DuplicateAddress:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Duplicate address found, can only send to each address once per send operation."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCreationFailed:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Error: Transaction creation failed. Please open Console and type 'clearwallettransactions' followed by 'scanforalltxns' to repair"),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::TransactionCommitFailed:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Error: The transaction was rejected. This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here. Please open Console and type 'clearwallettransactions' followed by 'scanforalltxns' to repair"),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::NarrationTooLong:
        QMessageBox::warning(this, tr("Send Coins"),
            tr("Error: Narration is too long."),
            QMessageBox::Ok, QMessageBox::Ok);
        break;
    case WalletModel::Aborted: // User aborted, nothing to do
        break;
    case WalletModel::OK:
        accept();
        CoinControlDialog::coinControl->UnSelectAll();
        coinControlUpdateLabels();
        break;
    }

    fWalletUnlockMintOnly = mintonly;
    model->updateStatus();

    fNewRecipientAllowed = true;
}

void SendCoinsDialog::clear()
{
    // Remove entries until only one left
    while(ui->entries->count())
    {
        delete ui->entries->takeAt(0)->widget();
    }
    addEntry();

    updateRemoveEnabled();

    ui->sendButton->setDefault(true);
}

void SendCoinsDialog::reject()
{
    clear();
}

void SendCoinsDialog::accept()
{
    clear();
}

SendCoinsEntry *SendCoinsDialog::addEntry()
{
    SendCoinsEntry *entry = new SendCoinsEntry(this);
    entry->setModel(model);
    ui->entries->addWidget(entry);
    connect(entry, SIGNAL(removeEntry(SendCoinsEntry*)), this, SLOT(removeEntry(SendCoinsEntry*)));
    connect(entry, SIGNAL(payAmountChanged()), this, SLOT(coinControlUpdateLabels()));

    updateRemoveEnabled();

    // Focus the field, so that entry can start immediately
    entry->clear();
    entry->setFocus();
    ui->scrollAreaWidgetContents->resize(ui->scrollAreaWidgetContents->sizeHint());

    QCoreApplication::instance()->processEvents();
    QScrollBar* bar = ui->scrollArea->verticalScrollBar();
    if(bar)
        bar->setSliderPosition(bar->maximum());
    return entry;
}

void SendCoinsDialog::updateRemoveEnabled()
{
    // Remove buttons are enabled as soon as there is more than one send-entry
    bool enabled = (ui->entries->count() > 1);
    for (int i = 0; i < ui->entries->count(); ++i) {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if (entry) {
            entry->setRemoveEnabled(enabled);

            QString qss = "SendCoinsEntry {background-color: ";
            qss += (i % 2 == 0) ? "#e4e4e4" : "#cccccc";
            qss += ";}";
            entry->setStyleSheet(qss);
        }
    }
    setupTabChain(0);
    coinControlUpdateLabels();
}

void SendCoinsDialog::removeEntry(SendCoinsEntry* entry)
{
    delete entry;
    updateRemoveEnabled();
}

QWidget *SendCoinsDialog::setupTabChain(QWidget *prev)
{
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
        {
            prev = entry->setupTabChain(prev);
        }
    }
    QWidget::setTabOrder(prev, ui->addButton);
    QWidget::setTabOrder(ui->addButton, ui->sendButton);
    return ui->sendButton;
}

void SendCoinsDialog::pasteEntry(const SendCoinsRecipient &rv)
{
    if(!fNewRecipientAllowed)
        return;

    SendCoinsEntry *entry = 0;
    // Replace the first entry if it is still unused
    if(ui->entries->count() == 1)
    {
        SendCoinsEntry *first = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(0)->widget());
        if(first->isClear())
        {
            entry = first;
        }
    }
    if(!entry)
    {
        entry = addEntry();
    }

    entry->setValue(rv);
}

bool SendCoinsDialog::handleURI(const QString &uri)
{
    SendCoinsRecipient rv;
    // URI has to be valid
    if (GUIUtil::parseBitcoinURI(uri, &rv))
    {
        CBitcoinAddress address(rv.address.toStdString());
        if (!address.IsValid())
            return false;
        pasteEntry(rv);
        return true;
    }

    return false;
}

void SendCoinsDialog::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance)
{
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate    things
    // for the non-mining users
    fShowImmature = (immatureBalance != 0);
    updateDisplayUnit();
    displayBannerInfo();
}

void SendCoinsDialog::setNumTransactions(int count)
{
    nCountOfTransactions = count;
    displayBannerInfo();
}


void SendCoinsDialog::updateDisplayUnit()
{
#if 1
    qint64 balance = model->getBalance();
#elif 0
    qint64 balance = (1000 + 100)*1000000;
#else
    qint64 balance = 888888888888888;
#endif

    if(model && model->getOptionsModel())
    {
        // Update labelBalance with the current balance and the current unit
        ui->labelBalance->setText(QString("Balance: ") + BitcoinUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), balance));
        displayBannerInfo();
    }
}

 // Coin Control: copy label "Quantity" to clipboard
 void SendCoinsDialog::coinControlClipboardQuantity()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlQuantity->text());
 }
 
 // Coin Control: copy label "Amount" to clipboard
 void SendCoinsDialog::coinControlClipboardAmount()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
 }
 
 // Coin Control: copy label "Fee" to clipboard
 void SendCoinsDialog::coinControlClipboardFee()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlFee->text().left(ui->labelCoinControlFee->text().indexOf(" ")));
 }
 
 // Coin Control: copy label "After fee" to clipboard
 void SendCoinsDialog::coinControlClipboardAfterFee()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")));
 }
 
 // Coin Control: copy label "Bytes" to clipboard
 void SendCoinsDialog::coinControlClipboardBytes()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlBytes->text());
 }
 
 // Coin Control: copy label "Priority" to clipboard
 void SendCoinsDialog::coinControlClipboardPriority()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlPriority->text());
 }
 
 // Coin Control: copy label "Low output" to clipboard
 void SendCoinsDialog::coinControlClipboardLowOutput()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlLowOutput->text());
 }
 
 // Coin Control: copy label "Change" to clipboard
 void SendCoinsDialog::coinControlClipboardChange()
 {
     QApplication::clipboard()->setText(ui->labelCoinControlChange->text().left(ui->labelCoinControlChange->text().indexOf(" ")));
 }
 
 // Coin Control: settings menu - coin control enabled/disabled by user
 void SendCoinsDialog::coinControlFeatureChanged(bool checked)
 {
     ui->frameCoinControl->setVisible(checked);
     ui->frmBanner->setVisible(!checked);
 
     if (!checked && model) // coin control features disabled
         CoinControlDialog::coinControl->SetNull();
 }
 
 // Coin Control: button inputs -> show actual coin control dialog
 void SendCoinsDialog::coinControlButtonClicked()
 {
     CoinControlDialog dlg;
     dlg.setModel(model);
     dlg.exec();
     coinControlUpdateLabels();
 }
 
 // Coin Control: checkbox custom change address
 void SendCoinsDialog::coinControlChangeChecked(int state)
 {
     if (model)
     {
         if (state == Qt::Checked)
             CoinControlDialog::coinControl->destChange = CBitcoinAddress(ui->lineEditCoinControlChange->text().toStdString()).Get();
         else
             CoinControlDialog::coinControl->destChange = CNoDestination();
     }
 
     ui->lineEditCoinControlChange->setEnabled((state == Qt::Checked));
     ui->labelCoinControlChangeLabel->setEnabled((state == Qt::Checked));
 }
 
 // Coin Control: custom change address changed
 void SendCoinsDialog::coinControlChangeEdited(const QString & text)
 {
     if (model)
     {
         CoinControlDialog::coinControl->destChange = CBitcoinAddress(text.toStdString()).Get();
 
         // label for the change address
         ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:black;}");
         if (text.isEmpty()) {
             ui->labelCoinControlChangeLabel->setText("");
         }
         else if (!CBitcoinAddress(text.toStdString()).IsValid()) {
             ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color:white;}");
             ui->labelCoinControlChangeLabel->setText(tr("WARNING: Invalid Bitcoin address"));
         }
         else {
             QString associatedLabel = model->getAddressTableModel()->labelForAddress(text);
             if (!associatedLabel.isEmpty()) {
                 ui->labelCoinControlChangeLabel->setText(associatedLabel);
             }
             else {
                 CPubKey pubkey;
                 CKeyID keyid;
                 CBitcoinAddress(text.toStdString()).GetKeyID(keyid);   
                 if (model->getPubKey(keyid, pubkey))
                     ui->labelCoinControlChangeLabel->setText(tr("(no label)"));
                 else
                 {
                     ui->labelCoinControlChangeLabel->setStyleSheet("QLabel{color: white;}");
                     ui->labelCoinControlChangeLabel->setText(tr("WARNING: unknown change address"));
                 }
             }
         }
     }
 }
 
 // Coin Control: update labels
 void SendCoinsDialog::coinControlUpdateLabels()
 {
     if (!model || !model->getOptionsModel() || !model->getOptionsModel()->getCoinControlFeatures())
         return;
     // set pay amounts
    CoinControlDialog::payAmounts.clear();
    for(int i = 0; i < ui->entries->count(); ++i)
    {
        SendCoinsEntry *entry = qobject_cast<SendCoinsEntry*>(ui->entries->itemAt(i)->widget());
        if(entry)
            CoinControlDialog::payAmounts.append(entry->getValue().amount);
    }
     if (CoinControlDialog::coinControl->HasSelected())
    {
        // actual coin control calculation
         CoinControlDialog::updateLabels(model, this);
        // show coin control stats
        ui->labelCoinControlAutomaticallySelected->hide();
        ui->widgetCoinControl->show();
    }
    else
    {
        // hide coin control stats
        ui->labelCoinControlAutomaticallySelected->show();
        ui->widgetCoinControl->hide();
        ui->labelCoinControlInsuffFunds->hide();
    }
}


void SendCoinsDialog::displayBannerInfo()
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
#elif 0
        BitcoinUnits::formatWithUnit(unit, 888888888888888),
        BitcoinUnits::formatWithUnit(unit, 888888888888888),
        BitcoinUnits::formatWithUnit(unit, 888888888888888),
        BitcoinUnits::formatWithUnit(unit, 888888888888888),
        BitcoinUnits::formatWithUnit(unit, 888888888888888),
        QString().setNum(88888888));
#endif

}


void SendCoinsDialog::customizeUI()
{
    if (model != NULL) {
        if (model->getOptionsModel() != NULL) {
            bool feature = model->getOptionsModel()->getCoinControlFeatures();
            QMessageBox::warning(0, "", feature ? "1" : "0");
        }
    }

    this->setWindowFlags(Qt::Widget);

    ui->frameCoinControl->setGeometry(0, 0, 825, 180);
    ui->frameCoinControl->setFixedSize(825, 180);
    ui->frameCoinControl->setStyleSheet(SC_QSS_TOOLTIP_DEFAULT + " QFrame {background-color: " + QString(SC_MAIN_COLOR_BLUE) + "; color: white; font-size: 15px; border: none;}");
    ui->widgetCoinControl->setStyleSheet(SC_QSS_TOOLTIP_DEFAULT + " QLabel {background-color: " + QString(SC_MAIN_COLOR_BLUE) + "; color: white; font-size: 12px; border: none;}");
    ui->labelCoinControlFeatures->setStyleSheet("QLabel {color: white; font-size: 25px; line-height: 30px; border: none;}");
    ui->labelCoinControlAutomaticallySelected->setStyleSheet("QLabel {color: white; font-size: 15px; line-height: 30px; border: none; qproperty-alignment: AlignRight;}");
    ui->labelCoinControlInsuffFunds->setStyleSheet("QLabel {color: white; font-size: 15px; line-height: 30px; border: none; qproperty-alignment: AlignRight;}");
    ui->pushButtonCoinControl->setStyleSheet("QPushButton {background-color: #666666; border: none; color: white; font-size: 15px; } QPushButton::hover {background-color: #3e3e41;}");

    QString qss = "QCheckBox {color: #fff; font-size: 15px; border: none; border-width: 0px; outline: none;}";
    qss += "QCheckBox::indicator { width: 30px; height: 30px; }";
    qss += "QCheckBox::indicator:unchecked {background-image: url(:/resources/res/images/icons-def/bg_checkbox.png);}";
    qss += "QCheckBox::indicator:checked {background-image: url(:/resources/res/images/icons-act/bg_checkbox.png);}";
    qss += "QCheckBox::indicator:hover {background-image: url(:/resources/res/images/icons-def/bg_checkbox_hover.png);}";
    qss += "QCheckBox::indicator:unchecked:pressed {background-image: url(:/resources/res/images/icons-def/bg_checkbox_pressed.png);}";
    qss += "QCheckBox::indicator:checked:pressed {background-image: url(:/resources/res/images/icons-def/bg_checkbox_pressed.png);}";
    qss += "QCheckBox::indicator:pressed {background-image: url(:/resources/res/images/icons-def/bg_checkbox_pressed.png);}";
    qss += "QCheckBox::indicator:hover:checked {background-image: url(:/resources/res/images/icons-def/bg_checkbox_hover.png);}";
    ui->checkBoxCoinControlChange->setStyleSheet(qss);

    ui->lineEditCoinControlChange->setStyleSheet("QLineEdit {background-color: #fff; border: none; color: black; padding-left: 0px 10px; font-size: 12px; }");

    txtBanner = new QStealthBlueInfo(STEALTH_PAGE_ID_SEND_XST, ui->frmBanner);

    qss = SC_QSS_TOOLTIP_DEFAULT + "QPushButton {background-image: url(";
    qss += SC_BTN_PATH_ACTIVE;
    qss += "bg_add_recipient.png); background-repeat: no-repeat; background-position: center; border: none;}";
    qss += "QPushButton::hover {background-image: url(";
    qss += SC_BTN_PATH_DEFAULT;
    qss += "bg_add_recipient.png);}";
    ui->addButton->setStyleSheet(qss);
    ui->addButton->setText("");

    qss = SC_QSS_TOOLTIP_DEFAULT + "QPushButton {background-image: url(";
    qss += SC_BTN_PATH_ACTIVE;
    qss += "bg_clear_all.png); background-repeat: no-repeat; background-position: center; border: none;}";
    qss += "QPushButton::hover {background-image: url(";
    qss += SC_BTN_PATH_DEFAULT;
    qss += "bg_clear_all.png);}";
    ui->clearButton->setStyleSheet(qss);
    ui->clearButton->setText("");

    ui->frmBalance->setFont(GUIUtil::stealthAppFont());
    ui->frmBalance->setStyleSheet("QFrame {background-color: #979797; color: #fff;}");

    // ui->frmBalance->resize(ui->frmBalance->width(), ui->clearButton->height());
    ui->frmBalance->setMaximumHeight(ui->clearButton->height());

    ui->sendButton->setFont(GUIUtil::stealthAppFont());
    // qss = SC_QSS_TOOLTIP_DEFAULT + "QPushButton {background-color: " + QString(SC_MAIN_COLOR_BLUE) + "; color: white; font-size: 15px; border: none; font-weight: bold;}";
    qss = SC_QSS_TOOLTIP_DEFAULT + "QPushButton {background-color: " + QString(SC_MAIN_COLOR_BLUE) + "; color: white; border: none; font-weight: bold;}";
    qss += "QPushButton::hover {background-color: #3e3e41;}";
    ui->sendButton->setStyleSheet(qss);


    // size the font dynamically for device compatibility
    SC_sizeFontButton(ui->sendButton, 0.72);

    ui->scrollArea->horizontalScrollBar()->setDisabled(true);
    ui->scrollArea->horizontalScrollBar()->hide();
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->scrollArea->setFixedHeight(300);

    qss = "";
    qss += "QScrollBar { border: none; }";
    qss += "QScrollBar::handle:vertical { background-color: #3e3e41; min-height:  40px;}";
    qss += "QScrollBar::sub-page:vertical { background-color: #999; }";
    qss += "QScrollBar::add-page:vertical { background-color: #999; }";
    ui->scrollArea->setStyleSheet(qss);

    ui->scrollAreaWidgetContents->setObjectName("scroll-area-widget");
    ui->scrollAreaWidgetContents->setStyleSheet("QWidget#scroll-area-widget {background-color: #ccc; border: none;}");

    qss = "SendCoinsDialog {border: none; background-color: #e4e4e4;}";
    qss += "QFrame { border: none; background-color: transparent;}";
    this->setStyleSheet(qss);
}
