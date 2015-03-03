#include "sendcoinsentry.h"
#include "ui_sendcoinsentry.h"
#include "guiutil.h"
#include "common/qstealth.h"
#include "bitcoinunits.h"
#include "addressbookpage.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "stealthaddress.h"

#include <QApplication>
#include <QClipboard>

SendCoinsEntry::SendCoinsEntry(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::SendCoinsEntry),
    model(0)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC
//    ui->payToLayout->setSpacing(4);
#endif
#if QT_VERSION >= 0x040700
    /* Do not move this to the XML file, Qt before 4.7 will choke on it */
    ui->addAsLabel->setPlaceholderText(tr("Enter a label for this address to add it to your address book"));
    ui->payTo->setPlaceholderText(tr("Enter a valid StealthCoin address"));
#endif
    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(ui->payTo);

    GUIUtil::setupAddressWidget(ui->payTo, this);

    customizeUI();
}

SendCoinsEntry::~SendCoinsEntry()
{
    delete ui;
}

void SendCoinsEntry::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void SendCoinsEntry::on_addressBookButton_clicked()
{
    if(!model)
        return;
    AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::SendingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if(dlg.exec())
    {
        ui->payTo->setText(dlg.getReturnValue());
        ui->payAmount->setFocus();
    }
}

void SendCoinsEntry::on_payTo_textChanged(const QString &address)
{
    if(!model)
        return;
    // Fill in label from address book, if address has an associated label
    QString associatedLabel = model->getAddressTableModel()->labelForAddress(address);
    if(!associatedLabel.isEmpty())
        ui->addAsLabel->setText(associatedLabel);
}

void SendCoinsEntry::setModel(WalletModel *model)
{
    this->model = model;

    if(model && model->getOptionsModel())
        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
		
		connect(ui->payAmount, SIGNAL(textChanged()), this, SIGNAL(payAmountChanged()));

    clear();
}

void SendCoinsEntry::setRemoveEnabled(bool enabled)
{
    QString tip = QCoreApplication::translate("bitcoin-core", "Remove this recipient");
    if (enabled) {
      ui->deleteButton->setToolTip(tip);
    } else {
      ui->deleteButton->setToolTip("");
    }
    ui->deleteButton->setEnabled(enabled);
}

void SendCoinsEntry::clear()
{
    ui->payTo->clear();
    ui->addAsLabel->clear();
    ui->payAmount->clear();
    ui->payTo->setFocus();
    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void SendCoinsEntry::on_deleteButton_clicked()
{
    emit removeEntry(this);
}

bool SendCoinsEntry::validate()
{
    // Check input validity
    bool retval = true;

    if(!ui->payAmount->validate())
    {
        retval = false;
    }
    else
    {
        if(ui->payAmount->value() <= 0)
        {
            // Cannot send 0 coins or less
            ui->payAmount->setValid(false);
            retval = false;
        }
    }

    if(!ui->payTo->hasAcceptableInput() ||
       (model && !model->validateAddress(ui->payTo->text())))
    {
        ui->payTo->setValid(false);
        retval = false;
    }

    return retval;
}

SendCoinsRecipient SendCoinsEntry::getValue()
{
    SendCoinsRecipient rv;

    rv.address = ui->payTo->text();
    if (rv.address.length() > 75
        && IsStealthAddress(rv.address.toStdString()))
        rv.typeInd = AddressTableModel::AT_Stealth;
    else
        rv.typeInd = AddressTableModel::AT_Normal;
    rv.label = ui->addAsLabel->text();
    rv.amount = ui->payAmount->value();

    return rv;
}

QWidget *SendCoinsEntry::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, ui->payTo);
    QWidget::setTabOrder(ui->payTo, ui->addressBookButton);
    QWidget::setTabOrder(ui->addressBookButton, ui->pasteButton);
    QWidget::setTabOrder(ui->pasteButton, ui->deleteButton);
    QWidget::setTabOrder(ui->deleteButton, ui->addAsLabel);

	return ui->payAmount->setupTabChain(ui->addAsLabel);
}

void SendCoinsEntry::setValue(const SendCoinsRecipient &value)
{
    ui->payTo->setText(value.address);
    ui->addAsLabel->setText(value.label);
    ui->payAmount->setValue(value.amount);
}

bool SendCoinsEntry::isClear()
{
    return ui->payTo->text().isEmpty();
}

void SendCoinsEntry::setFocus()
{
    ui->payTo->setFocus();
}

void SendCoinsEntry::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        // Update payAmount with the current unit
        ui->payAmount->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    }
}

void SendCoinsEntry::customizeUI()
{
    this->setWindowFlags(Qt::Widget);
    this->setStyleSheet("SendCoinsEntry {background-color: #e4e4e4;}");

    ui->lblPayTo->setStyleSheet("QLabel {background-color: #999; border: none; color: white; padding-left: 20px; qproperty-alignment: AlignVCenter;}");
    ui->payTo->makeTriAngle();

    ui->lblLabel->setStyleSheet("QLabel {background-color: #999; border: none; color: white; padding-left: 20px; qproperty-alignment: AlignVCenter;}");
    ui->addAsLabel->makeTriAngle();

    ui->lblAmount->setStyleSheet("QLabel {background-color: #999; border: none; color: white; padding-left: 20px; qproperty-alignment: AlignVCenter;}");
    ui->payAmount->makeTriAngle();

    QList<QLabel*> labels;
    labels << ui->lblPayTo << ui->lblLabel << ui->lblAmount;
    SC_sizeFontLabelGroup(labels, 0.7);

    ui->addressBookButton->setGeometry(315 + 0 * 160, 20, 150, 30);
    ui->pasteButton->setGeometry(315 + 1 * 160, 20, 150, 30);
    ui->deleteButton->setGeometry(315 + 2 * 160, 20, 150, 30);

    ui->addressBookButton->setIcon(QIcon());
    ui->pasteButton->setIcon(QIcon());
    ui->deleteButton->setIcon(QIcon());

    QString qss;

    qss = SC_QSS_TOOLTIP_DEFAULT;
    qss += "QToolButton { background-image: url(:/resources/res/images/btn-def/bg_entry_address.png); background-position: center; background-repeat: no-repeat; border: none;}";
    qss += "QToolButton::hover { background-image: url(:/resources/res/images/btn-act/bg_entry_address.png); background-position: center; background-repeat: no-repeat; border: none;}";
    ui->addressBookButton->setStyleSheet(qss);

    qss = SC_QSS_TOOLTIP_DEFAULT;
    qss += "QToolButton { background-image: url(:/resources/res/images/btn-def/bg_entry_paste.png); background-position: center; background-repeat: no-repeat; border: none;}";
    qss += "QToolButton::hover { background-image: url(:/resources/res/images/btn-act/bg_entry_paste.png); background-position: center; background-repeat: no-repeat; border: none;}";
    ui->pasteButton->setStyleSheet(qss);

    qss = SC_QSS_TOOLTIP_DEFAULT;
    qss += "QToolButton { background-image: url(:/resources/res/images/btn-def/bg_entry_remove.png); background-position: center; background-repeat: no-repeat; border: none;}";
    qss += "QToolButton::hover { background-image: url(:/resources/res/images/btn-act/bg_entry_remove.png); background-position: center; background-repeat: no-repeat; border: none;}";
    ui->deleteButton->setStyleSheet(qss);
}
