/*
 * Qt4 bitcoin GUI.
 *
 * W.J. van der Laan 2011-2012
 * The Bitcoin Developers 2011-2012
 */
#include "bitcoingui.h"
#include "transactiontablemodel.h"
#include "addressbookpage.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "optionsdialog.h"
#include "aboutdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "editaddressdialog.h"
#include "optionsmodel.h"
#include "transactiondescdialog.h"
#include "addresstablemodel.h"
#include "overviewpage.h"
#include "bitcoinunits.h"
#include "guiconstants.h"
#include "askpassphrasedialog.h"
#include "notificator.h"
#include "guiutil.h"
#include "rpcconsole.h"
#include "wallet.h"
#include "bitcoinrpc.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include <QApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QMessageBox>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#if QT_VERSION < 0x050000
   #include <QFileDialog> QT5 Conversion
   #include <QDesktopServices> QT5 Conversion
#else
   #include <QtWidgets>
#endif
#include <QDesktopWidget>
#include <QTimer>
#include <QDragEnterEvent>
#if QT_VERSION < 0x050000
#include <QUrl>
#endif
#include <QStyle>

#include <iostream>

#define TEST_STATUS 0

extern CWallet *pwalletMain;
extern int64 nLastCoinStakeSearchInterval;
extern unsigned int nStakeTargetSpacing;
extern bool fWalletUnlockMintOnly;

BitcoinGUI::BitcoinGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    encryptWalletAction(0),
    changePassphraseAction(0),
    lockWalletToggleAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0)
{
    QApplication::setStyle("windows");

    setGeometry(0, 0, SC_MAIN_WINDOW_WIDTH, SC_MAIN_WINDOW_HEIGHT);
    move(QApplication::desktop()->screen()->rect().center() - this->rect().center());

    mainMenu = new QStealthGrid(this);
    mainPage = new QStealthMain(this);
    bottomBar = new QBottomBar(this);

    signVerifyMessageDialog = new SignVerifyMessageDialog(this);

    setFixedSize(SC_MAIN_WINDOW_WIDTH, SC_MAIN_WINDOW_HEIGHT);

    connect(mainMenu, SIGNAL(selectedMenuItem(int)), this, SLOT(slotSelectedMenuItem(int)));
    connect(mainPage, SIGNAL(gotoMainPage(int)), this, SLOT(slotSelectedPageItem(int)));
  //connect(mainPage->sideBar, SIGNAL(clickedLogoImage()), this, SLOT(aboutClicked()));

    setWindowTitle(tr("Stealth") + " - " + tr("Wallet"));
#ifndef Q_OS_MAC
    qApp->setWindowIcon(QIcon(":icons/bitcoin"));
    setWindowIcon(QIcon(":icons/bitcoin"));
#else
    setUnifiedTitleAndToolBarOnMac(true);
    QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
    // Accept D&D of URIs
    setAcceptDrops(true);

    setObjectName("stealthWallet");
    setStyleSheet("#stealthWallet { background-color: qlineargradient(spread:pad, x1:0, y1:0, x2:1, y2:1, stop:0 #eeeeee, stop:1.0 #fefefe); } QToolTip { color: #cecece; background-color: #333333;  border:0px;} ");

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions();

    // Create application menu bar
    createMenuBar();
	
    // Create the tray icon (or setup the dock icon)
    createTrayIcon();

    // Status bar notification icons
//    labelEncryptionIcon = bottomBar->statusBar->lblIcons[0];
    labelMintingIcon = bottomBar->statusBar->lblIcons[SC_STATUS_ID_MINT];
    labelConnectionsIcon = bottomBar->statusBar->lblIcons[SC_STATUS_ID_CONNECTIONS];
    labelBlocksIcon = bottomBar->statusBar->lblIcons[SC_STATUS_ID_CHECK];

    // Set minting pixmap
    bottomBar->statusBar->setActive(SC_STATUS_ID_MINT, false);

    // Add timer to update minting icon
    QTimer *timerMintingIcon = new QTimer(labelMintingIcon);
    timerMintingIcon->start(MODEL_UPDATE_DELAY);
    connect(timerMintingIcon, SIGNAL(timeout()), this, SLOT(updateMintingIcon()));

    // Add timer to update minting weights
    QTimer *timerMintingWeights = new QTimer(labelMintingIcon);
    timerMintingWeights->start(30 * 1000);
    connect(timerMintingWeights, SIGNAL(timeout()), this, SLOT(updateMintingWeights()));

    // Set initial values for user and network weights
    nWeight = 0;
    nNetworkWeight = 0;

    //bottomBar->progressBar->hide();

    syncIconMovie = new QMovie(":/movies/update_spinner", "gif", this);
    // this->setStyleSheet("background-color: #effbef;");

    // Clicking on a transaction on the overview page simply sends you to transaction history page
    connect(mainPage->pageOverview, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
    connect(mainPage->pageOverview, SIGNAL(transactionClicked(QModelIndex)), mainPage->pageTransactions, SLOT(focusTransaction(QModelIndex)));

    // Double-clicking on a transaction on the transaction history page shows details
//    connect(mainPage->pageTransactions->table->table, SIGNAL(doubleClicked(QModelIndex)), this, mainPage->pageTransactions, SLOT(showDetails()));

    rpcConsole = new RPCConsole(this);
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));

    // Clicking on "Verify Message" in the address book sends you to the verify message tab
    connect(mainPage->pageAddrBook, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));
    // Clicking on "Sign Message" in the receive coins page sends you to the sign message tab
    connect(mainPage->pageReceiveXST, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));

    // HOME
    connect(mainPage, SIGNAL(gotoMainMenu()), this, SLOT(slotGoToMainMenu()));

    // FILE
    connect(mainPage, SIGNAL(fileBackup()), this, SLOT(backupWallet()));
  //connect(mainPage, SIGNAL(fileExport()), this, SLOT());
    connect(mainPage, SIGNAL(fileSign()), this, SLOT(gotoSignMessageTab()));
    connect(mainPage, SIGNAL(fileVerify()), this, SLOT(gotoVerifyMessageTab()));
    connect(mainPage, SIGNAL(fileExit()), qApp, SLOT(quit()));

    // SETTINGS
    connect(mainPage, SIGNAL(settingEncrypt(bool)), this, SLOT(encryptWallet(bool)));
    connect(mainPage, SIGNAL(settingPassphrase()), this, SLOT(changePassphrase()));
    connect(mainPage, SIGNAL(settingOptions()), this, SLOT(optionsClicked()));

    // HELP
    connect(mainPage, SIGNAL(helpDebugWindow()), rpcConsole, SLOT(show()));
    connect(mainPage, SIGNAL(helpAboutSC()), this, SLOT(aboutClicked()));
    connect(mainPage, SIGNAL(helpAboutQt()), qApp, SLOT(aboutQt()));

    // lock button slot
    connect(bottomBar->statusBar->btnLock, SIGNAL(clicked()), this, SLOT(lockBtnClicked()));

    slotGoToMainMenu();
}

BitcoinGUI::~BitcoinGUI()
{
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
#endif
}

void BitcoinGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setToolTip(tr("Show general overview of wallet"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send coins"), this);
    sendCoinsAction->setToolTip(tr("Send coins to an XST address"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive coins"), this);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), this);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(addressBookAction);

    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setToolTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(QIcon(":/icons/bitcoin"), tr("&About Stealth"), this);
    aboutAction->setToolTip(tr("Show information about Stealth"));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
    aboutQtAction->setToolTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setToolTip(tr("Modify configuration options for Stealth"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Show / Hide"), this);
    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setToolTip(tr("Encrypt or decrypt wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setToolTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setToolTip(tr("Change the passphrase used for wallet encryption"));
    lockWalletToggleAction = new QAction(this);
    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);

    exportAction = new QAction(QIcon(":/icons/export"), tr("&Export..."), this);
    exportAction->setToolTip(tr("Export the data in the current tab to a file"));
    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setToolTip(tr("Open debugging and diagnostic console"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(lockWalletToggleAction, SIGNAL(triggered()), this, SLOT(lockWalletToggle()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = NULL;
    return;
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->addAction(backupWalletAction);
    file->addAction(exportAction);
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addSeparator();
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    settings->addAction(encryptWalletAction);
    settings->addAction(changePassphraseAction);
    settings->addAction(lockWalletToggleAction);
    settings->addSeparator();
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);

    // QString ss("QMenuBar::item { background-color: #effbef; color: black }"); 
    // appMenuBar->setStyleSheet(ss);
}
void BitcoinGUI::setClientModel(ClientModel *clientModel)
{
    this->clientModel = clientModel;
    if(clientModel)
    {
        // Replace some strings and icons, when using the testnet
        if(clientModel->isTestNet())
        {
            setWindowTitle(windowTitle() + QString(" ") + tr("[testnet]"));
#ifndef Q_OS_MAC
            qApp->setWindowIcon(QIcon(":icons/bitcoin_testnet"));
            setWindowIcon(QIcon(":icons/bitcoin_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/bitcoin_testnet"));
#endif
            if(trayIcon)
            {
                trayIcon->setToolTip(tr("Stealth client") + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
                toggleHideAction->setIcon(QIcon(":/icons/toolbar_testnet"));
            }

            aboutAction->setIcon(QIcon(":/icons/toolbar_testnet"));
        }

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(setNumBlocks(int,int)));

        // Report errors from network/worker thread
        connect(clientModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        rpcConsole->setClientModel(clientModel);
        mainPage->pageAddrBook->setOptionsModel(clientModel->getOptionsModel());
        mainPage->pageReceiveXST->setOptionsModel(clientModel->getOptionsModel());
    }
}

void BitcoinGUI::setWalletModel(WalletModel *walletModel)
{
    this->walletModel = walletModel;
    if(walletModel)
    {
        // Report errors from wallet thread
        connect(walletModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));
		
		// set model
		mainPage->setModel(walletModel);

        signVerifyMessageDialog->setModel(walletModel);

        setEncryptionStatus(walletModel->getEncryptionStatus());
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(setEncryptionStatus(int)));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed (for a transaction)
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWalletForTxn()));
    }
}

void BitcoinGUI::createTrayIcon()
{
    QMenu *trayIconMenu;
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip(tr("Stealth client"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
        this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    trayIconMenu = dockIconHandler->dockMenu();
    dockIconHandler->setMainWindow((QMainWindow*)this);
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif

    notificator = new Notificator(qApp->applicationName(), trayIcon);
}

#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

void BitcoinGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;
    OptionsDialog dlg;
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void BitcoinGUI::aboutClicked()
{
    AboutDialog dlg;
    dlg.setModel(clientModel);
    dlg.exec();
}

void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to Stealth network.", "", count));
}

void BitcoinGUI::setNumBlocks(int count, int nTotalBlocks)
{
    // don't show / hide progress bar and its label if we have no connection to the network
    if (!clientModel || clientModel->getNumConnections() == 0)
    {
        bottomBar->progressBar->hide();
        bottomBar->progressBarLabel->hide();
#if TEST_STATUS
  bottomBar->progressBarLabel->setStyleSheet(QString(SC_QSS_PROGRESS_ALERT)
#if 0
                                            +"background-color:#00ffff;");
#else
  );
#endif
  bottomBar->progressBarLabel->setText(tr("This is a test message--please tell the dev..."));
  bottomBar->progressBarLabel->show();
  bottomBar->progressBar->setPercent(100);
  bottomBar->progressBar->show();
#endif
        return;
    }

    QString strStatusBarWarnings = clientModel->getStatusBarWarnings();
    QString tooltip;

    if(count < nTotalBlocks)
    {
        bottomBar->progressBar->show();

        // int nRemainingBlocks = nTotalBlocks - count;
        int nPercentageDone = (count == nTotalBlocks) ? 100 : 100 * count / nTotalBlocks;
        float fPercentageDone = count / (nTotalBlocks * 0.01f);

        if (strStatusBarWarnings.isEmpty())
        {
            bottomBar->progressBarLabel->setStyleSheet(QString(SC_QSS_PROGRESS_NORMAL));
            bottomBar->progressBarLabel->setText(tr("Synchronizing with network..."));
            bottomBar->progressBarLabel->setVisible(true);
#if TEST_STATUS
  bottomBar->progressBarLabel->setStyleSheet(QString(SC_QSS_PROGRESS_NORMAL)
#if 0
                                            +"background-color:#00aa00;");
#else
  );
#endif
  bottomBar->progressBarLabel->setText(tr("This is a test message--please tell the dev..."));
  bottomBar->progressBarLabel->show();
  bottomBar->progressBar->setPercent(100);
  bottomBar->progressBar->show();
#endif
//            bottomBar->progressBar->setFormat(tr("~%n block(s) remaining", "", nRemainingBlocks));
//            progressBar->setMaximum(nTotalBlocks);
//            progressBar->setValue(count);
//            progressBar->setVisible(true);
            bottomBar->progressBar->setPercent(nPercentageDone);
        }

        tooltip = tr("Downloaded %1 of %2 blocks of transaction history (%3% done).").arg(count).arg(nTotalBlocks).arg(fPercentageDone, 0, 'f', 2);
    }
    else
    {
        bottomBar->progressBar->hide();
        tooltip = tr("Downloaded %1 blocks of transaction history.").arg(count);

        if (strStatusBarWarnings.isEmpty()) {
             bottomBar->progressBarLabel->hide();
        }
    }

    // Override progressBarLabel text when we have warnings to display
    if (!strStatusBarWarnings.isEmpty())
    {
        bottomBar->progressBarLabel->setText(strStatusBarWarnings);
        bottomBar->progressBarLabel->show();
        bottomBar->progressBarLabel->setStyleSheet(QString(SC_QSS_PROGRESS_ALERT));
    }

#if TEST_STATUS
  bottomBar->progressBarLabel->setStyleSheet(QString(SC_QSS_PROGRESS_ALERT)
#if 0
                                            +"background-color:#00ffff;");
#else
  );
#endif
  bottomBar->progressBarLabel->setText(tr("This is a test message--please tell the dev..."));
  bottomBar->progressBarLabel->show();
  bottomBar->progressBar->setPercent(100);
  bottomBar->progressBar->show();
#endif

    tooltip = tr("Current difficulty is %1.").arg(clientModel->GetDifficulty()) + QString("<br>") + tooltip;

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int secs = lastBlockDate.secsTo(QDateTime::currentDateTime());
    QString text;

    // Represent time from last generated block in human readable text
    if(secs <= 0)
    {
        // Fully up to date. Leave text empty.
    }
    else if(secs < 60)
    {
        text = tr("%n second(s) ago","",secs);
    }
    else if(secs < 60*60)
    {
        text = tr("%n minute(s) ago","",secs/60);
    }
    else if(secs < 24*60*60)
    {
        text = tr("%n hour(s) ago","",secs/(60*60));
    }
    else
    {
        text = tr("%n day(s) ago","",secs/(60*60*24));
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
//        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelBlocksIcon->setPixmap(QPixmap(":/resources/res/images/icons-act/ico_check.png"));

        mainPage->showOutOfSyncWarning(false);
    }
    else
    {
        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        labelBlocksIcon->setMovie(syncIconMovie);
        syncIconMovie->start();

        mainPage->showOutOfSyncWarning(true);
    }

    if(!text.isEmpty())
    {
        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1.").arg(text);
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    bottomBar->progressBarLabel->setToolTip(tooltip);
    bottomBar->progressBar->setToolTip(tooltip);
}

void BitcoinGUI::error(const QString &title, const QString &message, bool modal)
{
    // Report errors from network/worker thread
    if(modal)
    {
        QMessageBox::critical(this, title, message, QMessageBox::Ok, QMessageBox::Ok);
    } else {
        notificator->notify(Notificator::Critical, title, message);
    }
}

void BitcoinGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent *event)
{
    if(clientModel)
    {
#ifndef Q_OS_MAC // Ignored on Mac
        if(!clientModel->getOptionsModel()->getMinimizeToTray() &&
            !clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            qApp->quit();
        }
#endif
    }
    QMainWindow::closeEvent(event);
}

void BitcoinGUI::askFee(qint64 nFeeRequired, bool *payFee)
{
    QString strMessage =
        tr("This transaction is over the size limit.  You can still send it for a fee of %1, "
        "which goes to the nodes that process your transaction and helps to support the network.  "
        "Do you want to pay the fee?").arg(
        BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
        this, tr("Confirm transaction fee"), strMessage,
        QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

void BitcoinGUI::incomingTransaction(const QModelIndex & parent, int start, int end)
{
    if(!walletModel || !clientModel)
        return;
    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
        .data(Qt::EditRole).toULongLong();
    if(!clientModel->inInitialBlockDownload())
    {
        // On new transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date = ttm->index(start, TransactionTableModel::Date, parent)
            .data().toString();
        QString type = ttm->index(start, TransactionTableModel::Type, parent)
            .data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent)
            .data().toString();
        QIcon icon = qvariant_cast<QIcon>(ttm->index(start,
            TransactionTableModel::ToAddress, parent)
            .data(Qt::DecorationRole));

        notificator->notify(Notificator::Information,
            (amount)<0 ? tr("Sent transaction") :
            tr("Incoming transaction"),
            tr("Date: %1\n"
            "Amount: %2\n"
            "Type: %3\n"
            "Address: %4\n")
            .arg(date)
            .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
            .arg(type)
            .arg(address), icon);
    }
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    showSelectedPageItem(SC_PAGE_ID_OVERVIEW);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    disconnect(mainPage, SIGNAL(fileExport()), 0, 0);
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    showSelectedPageItem(SC_PAGE_ID_TRANSACTIONS);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), mainPage->pageTransactions, SLOT(exportClicked()));
}

void BitcoinGUI::gotoAddressBookPage()
{
    addressBookAction->setChecked(true);
    showSelectedPageItem(SC_PAGE_ID_ADDR_BOOK);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), mainPage->pageAddrBook, SLOT(exportClicked()));
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    showSelectedPageItem(SC_PAGE_ID_RECEIVE_XST);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), mainPage->pageReceiveXST, SLOT(exportClicked()));
}

void BitcoinGUI::gotoSendCoinsPage()
{
    sendCoinsAction->setChecked(true);
    showSelectedPageItem(SC_PAGE_ID_SEND_XST);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if(!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void BitcoinGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        int nValidUrisFound = 0;
        QList<QUrl> uris = event->mimeData()->urls();
        foreach(const QUrl &uri, uris)
        {
            if (mainPage->pageSendXST->sendCoinsPage->handleURI(uri.toString()))
                nValidUrisFound++;
        }

        // if valid URIs were found
        if (nValidUrisFound)
            slotSelectedPageItem(SC_PAGE_ID_SEND_XST);
        else
            notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid XST address or malformed URI parameters."));
    }

    event->acceptProposedAction();
}

void BitcoinGUI::handleURI(QString strURI)
{
    // URI has to be valid
    if (mainPage->pageSendXST->sendCoinsPage->handleURI(strURI))
    {
        showNormalIfMinimized();
        slotSelectedPageItem(SC_PAGE_ID_SEND_XST);
    }
    else
        notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid XST address or malformed URI parameters."));
}

void BitcoinGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
//      labelEncryptionIcon->hide();
        bottomBar->statusBar->btnLock->setToolTip(tr("Wallet is <b>unencrypted</b>! For the security of your funds, please <b>encrypt</b> it."));
        encryptWalletAction->setChecked(false);
        encryptWalletAction->setEnabled(true);
        changePassphraseAction->setEnabled(false);
        lockWalletToggleAction->setVisible(false);
        bottomBar->statusBar->setLockState(SC_LS_UNENCRYPTED);
        break;
    case WalletModel::Unlocked:
//      labelEncryptionIcon->show();
        bottomBar->statusBar->btnLock->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>."));
        if (fWalletUnlockMintOnly) {
            bottomBar->statusBar->setLockState(SC_LS_MINT_ONLY);
        } else {
            bottomBar->statusBar->setLockState(SC_LS_UNLOCKED);
        }
        encryptWalletAction->setChecked(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        changePassphraseAction->setEnabled(true);
        lockWalletToggleAction->setVisible(true);
        lockWalletToggleAction->setIcon(QIcon(":/icons/lock_open"));
        lockWalletToggleAction->setText(tr("&Lock Wallet"));
        lockWalletToggleAction->setToolTip(tr("Lock wallet"));
        break;
    case WalletModel::Locked:
//      labelEncryptionIcon->show();
        bottomBar->statusBar->btnLock->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>."));
        bottomBar->statusBar->setLockState(SC_LS_LOCKED);
        encryptWalletAction->setChecked(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        changePassphraseAction->setEnabled(true);
        lockWalletToggleAction->setVisible(true);
        lockWalletToggleAction->setIcon(QIcon(":/icons/lock_closed"));
        lockWalletToggleAction->setText(tr("&Unlock Wallet..."));
        lockWalletToggleAction->setToolTip(tr("Unlock wallet"));
        break;
    }
}

void BitcoinGUI::encryptWallet(bool status)
{
    if(!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt:
        AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus(walletModel->getEncryptionStatus());
}

void BitcoinGUI::backupWallet()
{
#if QT_VERSION < 0x050000
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    QString filename = QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if(!filename.isEmpty()) {
        if(!walletModel->backupWallet(filename)) {
            QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."));
        }
    }
}

void BitcoinGUI::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void BitcoinGUI::lockWalletToggle()
{
    if(!walletModel)
        return;

    // Unlock wallet when requested by wallet model
    if(walletModel->getEncryptionStatus() == WalletModel::Locked)
    {
        AskPassphraseDialog::Mode mode = AskPassphraseDialog::UnlockMinting;
        AskPassphraseDialog dlg(mode, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
    else
        walletModel->setWalletLocked(true);
}

void BitcoinGUI::unlockWalletForTxn() {
  if (!walletModel) {
       return;
  }
  // Unlock wallet when requested by wallet model
  if ((walletModel->getEncryptionStatus() == WalletModel::Locked) ||
       fWalletUnlockMintOnly)
  {
      AskPassphraseDialog::Mode mode = AskPassphraseDialog::UnlockTxn;
      AskPassphraseDialog dlg(mode, this);
      dlg.setModel(walletModel);
      dlg.exec();
  }
}

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if (GUIUtil::isObscured(this))
    {
        raise();
        activateWindow();
    }
    else if(fToggleHidden)
        hide();
}

void BitcoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BitcoinGUI::updateMintingIcon()
{
    if (pwalletMain && pwalletMain->IsLocked())
    {
        labelMintingIcon->setToolTip(tr("Not minting because wallet is locked."));
//        labelMintingIcon->setEnabled(false);
        bottomBar->statusBar->setActive(SC_STATUS_ID_MINT, false);
    }
    else if (vNodes.empty())
    {
        labelMintingIcon->setToolTip(tr("Not minting because wallet is offline."));
//        labelMintingIcon->setEnabled(false);
        bottomBar->statusBar->setActive(SC_STATUS_ID_MINT, false);
    }
    else if (IsInitialBlockDownload())
    {
        labelMintingIcon->setToolTip(tr("Not minting because wallet is syncing."));
//        labelMintingIcon->setEnabled(false);
        bottomBar->statusBar->setActive(SC_STATUS_ID_MINT, false);
    }
    else if (!nWeight)
    {
        labelMintingIcon->setToolTip(tr("Not minting because you don't have mature coins."));
//        labelMintingIcon->setEnabled(false);
        bottomBar->statusBar->setActive(SC_STATUS_ID_MINT, false);
    }
    else if (nLastCoinStakeSearchInterval)
    {
        uint64 nEstimateTime = nStakeTargetSpacing * nNetworkWeight / nWeight;

        QString text;
        if (nEstimateTime < 60)
        {
            text = tr("%n second(s)", "", nEstimateTime);
        }
        else if (nEstimateTime < 60*60)
        {
            text = tr("%n minute(s)", "", nEstimateTime/60);
        }
        else if (nEstimateTime < 24*60*60)
        {
            text = tr("%n hour(s)", "", nEstimateTime/(60*60));
        }
        else
        {
            text = tr("%n day(s)", "", nEstimateTime/(60*60*24));
        }

//        labelMintingIcon->setEnabled(true);
        labelMintingIcon->setToolTip(tr("Minting.<br>Your weight is %1.<br>Network weight is %2.<br>Expected time to earn reward is within %3.").arg(nWeight).arg(nNetworkWeight).arg(text));
        bottomBar->statusBar->setActive(SC_STATUS_ID_MINT, true);
    }
    else
    {
        labelMintingIcon->setToolTip(tr("Not minting."));
//        labelMintingIcon->setEnabled(false);
        bottomBar->statusBar->setActive(SC_STATUS_ID_MINT, false);
    }
}

void BitcoinGUI::updateMintingWeights()
{
    // Only update if we have the network's current number of blocks, or weight(s) are zero (fixes lagging GUI)
    if ((clientModel && clientModel->getNumBlocks() == clientModel->getNumBlocksOfPeers()) || !nWeight || !nNetworkWeight)
    {
        nWeight = 0;

        if (pwalletMain)
            pwalletMain->GetStakeWeight(*pwalletMain, nMinMax, nMinMax, nWeight);

        nNetworkWeight = GetPoSKernelPS();
    }
}

void BitcoinGUI::slotGoToMainMenu()
{
    mainPage->hide();
    mainMenu->show();
}

void BitcoinGUI::slotSelectedMenuItem(int id)
{

    // disable incomplete pages
    // disabled also in QStealthMain::showPage
    switch(id) {
      case SC_PAGE_ID_INCOME_EXPENSE:
      case SC_PAGE_ID_STATISTICS:
      case SC_PAGE_ID_SOCIAL:
        return;
    }

    mainMenu->hide();
    slotSelectedPageItem(id);
}

void BitcoinGUI::slotSelectedPageItem(int id)
{
    if (id < 0 || id > 8) return;

    SC_PAGE_ID page_id = (SC_PAGE_ID)id;

    switch (page_id)
    {
    case SC_PAGE_ID_OVERVIEW:
        showSelectedPageItem(page_id);
        exportAction->setEnabled(false);
        disconnect(exportAction, SIGNAL(triggered()), 0, 0);
        disconnect(mainPage, SIGNAL(fileExport()), 0, 0);
        break;

    case SC_PAGE_ID_SEND_XST:
        showSelectedPageItem(page_id);
        exportAction->setEnabled(false);
        disconnect(exportAction, SIGNAL(triggered()), 0, 0);
        break;

    case SC_PAGE_ID_RECEIVE_XST:
        showSelectedPageItem(page_id);
        exportAction->setEnabled(true);
        disconnect(exportAction, SIGNAL(triggered()), 0, 0);
        connect(exportAction, SIGNAL(triggered()), mainPage->pageReceiveXST, SLOT(exportClicked()));
        break;

    case SC_PAGE_ID_TRANSACTIONS:
        showSelectedPageItem(page_id);
        exportAction->setEnabled(true);
        disconnect(exportAction, SIGNAL(triggered()), 0, 0);
        connect(exportAction, SIGNAL(triggered()), mainPage->pageTransactions, SLOT(exportClicked()));
        break;

    case SC_PAGE_ID_ADDR_BOOK:
        showSelectedPageItem(page_id);
        exportAction->setEnabled(true);
        disconnect(exportAction, SIGNAL(triggered()), 0, 0);
        connect(exportAction, SIGNAL(triggered()), mainPage->pageAddrBook, SLOT(exportClicked()));
        break;

    case SC_PAGE_ID_INCOME_EXPENSE:
    case SC_PAGE_ID_STATISTICS:
    case SC_PAGE_ID_BLOCK_EXP:
    case SC_PAGE_ID_SOCIAL:
        showSelectedPageItem(page_id);
        exportAction->setEnabled(false);
        disconnect(exportAction, SIGNAL(triggered()), 0, 0);
        break;

    default:
        ;
    }
}

void BitcoinGUI::showSelectedPageItem(SC_PAGE_ID id)
{
    mainPage->show((SC_PAGE_ID)id);
}

void BitcoinGUI::mousePressEvent(QMouseEvent *)
{
    mainPage->hideAllPopups();
}

void BitcoinGUI::lockBtnClicked()
{
//    if (bottomBar == NULL) return;
//    if (bottomBar->statusBar == NULL) return;
//    bool fLockState = bottomBar->statusBar->lockState();
//    QMessageBox::warning(0, "Current Lock State", fLockState ? "Locked" : "Unlocked");

    if (walletModel->getEncryptionStatus() ==  WalletModel::Unencrypted) {
        encryptWallet(true);
    } else {
        lockWalletToggle();
    }
}
