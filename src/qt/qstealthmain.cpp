#include "qstealthmain.h"
#include "common/qstealth.h"
#include <QApplication>
#include <QDesktopWidget>

QStealthMain::QStealthMain(QWidget *parent)
    : QLabel(parent)
{
    setGeometry(0, 0, SC_MAIN_WINDOW_WIDTH, SC_MAIN_WINDOW_HEIGHT);
    setStyleSheet("QStealthMain {background-color: #000;}");

    titleBar = new QTitleBar(this);
    sideBar = new QSideMenuBar(this);
    sideBar->btn[0]->setState(SC_BUTTON_STATE_CLICKED);

    int k = 0;
    pages[k++] = pageOverview = new QPageOverview(this);
    pages[k++] = pageSendXST = new QPageSendXST(this);
    pages[k++] = pageReceiveXST = new QPageAddrBook(QPageAddrBook::ForEditing, QPageAddrBook::ReceivingTab, this);
    pages[k++] = pageTransactions = new QPageTransactions(this);
    pages[k++] = pageAddrBook = new QPageAddrBook(QPageAddrBook::ForEditing, QPageAddrBook::SendingTab, this);
    pages[k++] = pageIncomeExpense = new QPageIncomeExpense(this);
    // pages[k++] = pageBlockExp = new QPageBlockExp(this);
    pages[k++] = blockBrowser = new BlockBrowser(this);
    pages[k++] = pageStatistics = new QPageStatistics(this);
    pages[k++] = pageSocial = new QPageSocial(this);

    connect(sideBar, SIGNAL(clicked(int)), this, SLOT(slotSideBarBtnClicked(int)));
    connect(titleBar, SIGNAL(gotoMainMenu()), this, SLOT(slotGoToMainMenu()));

    // create all pages

    // last step
    QHoverButton *button;

    popupHome = new QTitlePopup(this);
    popupHome->setGeometry(676, 70 - 9, 200, 39);

    button = new QHoverButton(popupHome, 101);
    button->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_return.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_return.png); background-position: center; background-repeat: no-repeat;");
    popupHome->insertItem(button);
    connect(button, SIGNAL(clicked()), this, SLOT(slotGoToMainMenu()));

    popupFile = new QTitlePopup(this);
    popupFile->setGeometry(725, 70 - 9, 200, 39);

    button = new QHoverButton(popupFile, 201);
    button->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_backup.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_backup.png); background-position: center; background-repeat: no-repeat;");
    popupFile->insertItem(button);
    connect(button, SIGNAL(clicked()), this, SLOT(slotFileBackup()));

    button = new QHoverButton(popupFile, 202);
    button->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_export.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_export.png); background-position: center; background-repeat: no-repeat;");
    popupFile->insertItem(button);
    connect(button, SIGNAL(clicked()), this, SLOT(slotFileExport()));

    button = new QHoverButton(popupFile, 203);
    button->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_sign.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_sign.png); background-position: center; background-repeat: no-repeat;");
    popupFile->insertItem(button);
    connect(button, SIGNAL(clicked()), this, SLOT(slotFileSign()));

    button = new QHoverButton(popupFile, 204);
    button->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_verify.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_verify.png); background-position: center; background-repeat: no-repeat;");
    popupFile->insertItem(button);
    connect(button, SIGNAL(clicked()), this, SLOT(slotFileVerify()));

    button = new QHoverButton(popupFile, 205);
    button->setFrontStyleSheet(
#ifdef Q_OS_MAC
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_exit_mac.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_exit_mac.png); background-position: center; background-repeat: no-repeat;");
#else
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_exit.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_exit.png); background-position: center; background-repeat: no-repeat;");
#endif
    popupFile->insertItem(button);
    connect(button, SIGNAL(clicked()), this, SLOT(slotFileExit()));

    popupSettings = new QTitlePopup(this);
    popupSettings->setGeometry(795, 70 - 9, 200, 39);


    this->qhbtnEncrypt = new QHoverButton(popupSettings, 301);

    this->qhbtnEncrypt->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_encrypt.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_encrypt.png); background-position: center; background-repeat: no-repeat;");
    popupSettings->insertItem(this->qhbtnEncrypt);
    connect(this->qhbtnEncrypt, SIGNAL(clicked()), this, SLOT(slotSettingEncrypt()));


    this->qhbtnChangePass = new QHoverButton(popupSettings, 302);
    this->qhbtnChangePass->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_change.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_change.png); background-position: center; background-repeat: no-repeat;");
    popupSettings->insertItem(this->qhbtnChangePass);
    connect(this->qhbtnChangePass, SIGNAL(clicked()), this, SLOT(slotSettingPassphrase()));

    button = new QHoverButton(popupSettings, 303);
    button->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_options.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_options.png); background-position: center; background-repeat: no-repeat;");
    popupSettings->insertItem(button);
    connect(button, SIGNAL(clicked()), this, SLOT(slotSettingOptions()));

    popupHelp = new QTitlePopup(this);
    popupHelp->setGeometry(870, 70 - 9, 200, 39);

    button = new QHoverButton(popupHelp, 401);
    button->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_debug.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_debug.png); background-position: center; background-repeat: no-repeat;");
    popupHelp->insertItem(button);
    connect(button, SIGNAL(clicked()), this, SLOT(slotHelpDebugWindow()));

    button = new QHoverButton(popupHelp, 402);
    button->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_about_sc.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_about_sc.png); background-position: center; background-repeat: no-repeat;");
    popupHelp->insertItem(button);
    connect(button, SIGNAL(clicked()), this, SLOT(slotHelpAboutSC()));

    button = new QHoverButton(popupHelp, 403);
    button->setFrontStyleSheet(
        QString("background-image: url(") + SC_POPUP_PATH_DEFAULT + "bg_menu_about_qt.png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_POPUP_PATH_ACTIVE + "bg_menu_about_qt.png); background-position: center; background-repeat: no-repeat;");
    popupHelp->insertItem(button);
    connect(button, SIGNAL(clicked()), this, SLOT(slotHelpAboutQt()));

    // connect(titleBar->btnHome, SIGNAL(clicked()), this, SLOT(slotClickedHome()));
    connect(titleBar->btnHome, SIGNAL(clicked()), this, SLOT(slotGoToMainMenu()));
    connect(titleBar->btnFile, SIGNAL(clicked()), this, SLOT(slotClickedFile()));
    connect(titleBar->btnSettings, SIGNAL(clicked()), this, SLOT(slotClickedSettings()));
    connect(titleBar->btnHelp, SIGNAL(clicked()), this, SLOT(slotClickedHelp()));
}

QStealthMain::~QStealthMain()
{

}

void QStealthMain::setModel(WalletModel *model)
{
    this->model = model;
    pageOverview->setModel(model);
    pageSendXST->setModel(model);
    pageReceiveXST->setModel(model->getAddressTableModel());
    pageTransactions->setModel(model);
    pageAddrBook->setModel(model->getAddressTableModel());
    pageIncomeExpense->setModel(model);
//    pageBlockExp->setModel(model);
//    pageStatistics->setModel(model);
//    pageSocial->setModel(model);
}

void QStealthMain::showOutOfSyncWarning(bool fShow)
{
    pageOverview->showOutOfSyncWarning(fShow);
    pageSendXST->showOutOfSyncWarning(fShow);
    pageIncomeExpense->showOutOfSyncWarning(fShow);
}

void QStealthMain::show(SC_PAGE_ID id)
{
    showPage(id);
    QWidget::show();
}

void QStealthMain::showPage(SC_PAGE_ID id)
{
    // disable incomplete pages (needed for tile page)
    // disabled also in BitcoinGUI::slotSelectedMenuItem
    switch(id) {
       case SC_PAGE_ID_OVERVIEW :
       case SC_PAGE_ID_SEND_XST :
       case SC_PAGE_ID_RECEIVE_XST :
       case SC_PAGE_ID_TRANSACTIONS :
       case SC_PAGE_ID_ADDR_BOOK :
       case SC_PAGE_ID_BLOCK_EXP :
         break ;
       case SC_PAGE_ID_INCOME_EXPENSE :
       case SC_PAGE_ID_STATISTICS :
       case SC_PAGE_ID_SOCIAL :
         return ;
    }

    pageID = id;
    titleBar->setTitleInfo(id);
    for (int i = 0; i < SC_SIDE_BTN_COUNT; i++) {
        sideBar->btn[i]->setState(i == id ? SC_BUTTON_STATE_CLICKED : SC_BUTTON_STATE_DEFAULT);
        (i == id) ? pages[i]->show() : pages[i]->hide();
    }
    hideAllPopups();
}

void QStealthMain::hideAllPopups()
{
    popupHome->hide();
    popupFile->hide();
    popupSettings->hide();
    popupHelp->hide();
}

void QStealthMain::slotSideBarBtnClicked(int id)
{
    emit gotoMainPage(id);
}

void QStealthMain::slotGoToMainMenu()
{
    emit gotoMainMenu();
}

void QStealthMain::slotFileBackup()
{
    emit fileBackup();
}

void QStealthMain::slotFileExport()
{
    switch (pageID)
    {
    case SC_PAGE_ID_OVERVIEW:
        break;

    case SC_PAGE_ID_SEND_XST:
        break;

    case SC_PAGE_ID_RECEIVE_XST:
        pageReceiveXST->exportClicked();
        break;

    case SC_PAGE_ID_TRANSACTIONS:
        pageTransactions->exportClicked();
        break;

    case SC_PAGE_ID_ADDR_BOOK:
        pageAddrBook->exportClicked();
        break;

    case SC_PAGE_ID_INCOME_EXPENSE:
        break;

    case SC_PAGE_ID_BLOCK_EXP:
        break;

    case SC_PAGE_ID_STATISTICS:
        break;

    case SC_PAGE_ID_SOCIAL:
        break;

    default:
        ;
    }
}

void QStealthMain::slotFileSign()
{
    emit fileSign();
}

void QStealthMain::slotFileVerify()
{
    emit fileVerify();
}

void QStealthMain::slotFileExit()
{
    emit fileExit();
}

void QStealthMain::slotSettingEncrypt()
{
    emit settingEncrypt(true);
}

void QStealthMain::slotSettingPassphrase()
{
    emit settingPassphrase();
}

void QStealthMain::slotSettingOptions()
{
    emit settingOptions();
}

void QStealthMain::slotHelpDebugWindow()
{
    emit helpDebugWindow();
}

void QStealthMain::slotHelpAboutSC()
{
    emit helpAboutSC();
}

void QStealthMain::slotHelpAboutQt()
{
    emit helpAboutQt();
}

void QStealthMain::slotClickedHome()
{
    (popupHome->isVisible()) ? popupHome->hide() : popupHome->show();

    popupFile->hide();
    popupSettings->hide();
    popupHelp->hide();
}

void QStealthMain::slotClickedFile()
{
    (popupFile->isVisible()) ? popupFile->hide() : popupFile->show();

    popupHome->hide();
    popupSettings->hide();
    popupHelp->hide();
}

void QStealthMain::slotClickedSettings()
{
    WalletModel::EncryptionStatus status = this->model->getEncryptionStatus();

    switch(status) {
      case WalletModel::Unencrypted :
         this->qhbtnEncrypt->setFrontStyleSheet(
            QString("background-image: url(") + SC_POPUP_PATH_DEFAULT +
                    "bg_menu_encrypt.png); background-position: center; background-repeat: no-repeat;",
            QString("background-image: url(") + SC_POPUP_PATH_ACTIVE +
                    "bg_menu_encrypt.png); background-position: center; background-repeat: no-repeat;");
            this->qhbtnEncrypt->setEnabled(true);
         this->qhbtnChangePass->setFrontStyleSheet(
            QString("background-image: url(") + SC_POPUP_PATH_INACTIVE +
                    "bg_menu_change.png); background-position: center; background-repeat: no-repeat;",
            QString("background-image: url(") + SC_POPUP_PATH_INACTIVE +
                    "bg_menu_change.png); background-position: center; background-repeat: no-repeat;");
            this->qhbtnChangePass->setEnabled(false);
         break;
      case WalletModel::Locked :
      case WalletModel::Unlocked :
         this->qhbtnEncrypt->setFrontStyleSheet(
            QString("background-image: url(") + SC_POPUP_PATH_INACTIVE +
                    "bg_menu_encrypt.png); background-position: center; background-repeat: no-repeat;",
            QString("background-image: url(") + SC_POPUP_PATH_INACTIVE +
                    "bg_menu_encrypt.png); background-position: center; background-repeat: no-repeat;");
            this->qhbtnEncrypt->setEnabled(false);
         this->qhbtnChangePass->setFrontStyleSheet(
            QString("background-image: url(") + SC_POPUP_PATH_DEFAULT +
                    "bg_menu_change.png); background-position: center; background-repeat: no-repeat;",
            QString("background-image: url(") + SC_POPUP_PATH_ACTIVE +
                    "bg_menu_change.png); background-position: center; background-repeat: no-repeat;");
            this->qhbtnChangePass->setEnabled(true);
    }

    (popupSettings->isVisible()) ? popupSettings->hide() : popupSettings->show();

    popupHome->hide();
    popupFile->hide();
    popupHelp->hide();
}

void QStealthMain::slotClickedHelp()
{
    (popupHelp->isVisible()) ? popupHelp->hide() : popupHelp->show();

    popupHome->hide();
    popupFile->hide();
    popupSettings->hide();
}
