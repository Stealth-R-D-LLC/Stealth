#include "qpagesendxst.h"

QPageSendXST::QPageSendXST(QWidget *parent) :
    QStealthPage(parent),
    model(0)
{
    sendCoinsPage = new SendCoinsDialog(this);
    sendCoinsPage->setGeometry(0, 0, SC_PAGE_WIDTH, SC_PAGE_HEIGHT);
    sendCoinsPage->showNormal();
}

void QPageSendXST::setModel(WalletModel *model)
{
    sendCoinsPage->setModel(model);
}

void QPageSendXST::showOutOfSyncWarning(bool fShow)
{
    sendCoinsPage->showOutOfSyncWarning(fShow);
}

