#ifndef QSTEALTHMAIN_H
#define QSTEALTHMAIN_H

#include <QLabel>
#include "qtitlebar.h"
#include "qtitlepopup.h"
#include "qsidemenubar.h"

#include "qstealthpage.h"
#include "qpageoverview.h"
#include "qpagesendxst.h"
#include "qpagetransactions.h"
#include "qpageaddrbook.h"
#include "qpageincomeexpense.h"
// #include "qpageblockexp.h"
#include "blockbrowser.h"
#include "qpagestatistics.h"
#include "qpagesocial.h"

#include "walletmodel.h"

#define SC_PAGE_COUNT               9
#define SC_SIDE_BTN_COUNT           SC_PAGE_COUNT

typedef enum {
    SC_PAGE_ID_OVERVIEW = 0,
    SC_PAGE_ID_SEND_XST,
    SC_PAGE_ID_RECEIVE_XST,
    SC_PAGE_ID_TRANSACTIONS,
    SC_PAGE_ID_ADDR_BOOK,
    SC_PAGE_ID_INCOME_EXPENSE,
    SC_PAGE_ID_BLOCK_EXP,
    SC_PAGE_ID_STATISTICS,
    SC_PAGE_ID_SOCIAL
} SC_PAGE_ID;

class QStealthMain : public QLabel
{
    Q_OBJECT

public:
    QStealthMain(QWidget *parent = 0);
    ~QStealthMain();

    void setModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);

    void show(SC_PAGE_ID id);
    void showPage(SC_PAGE_ID id);
    void hideAllPopups();

signals:
    void gotoMainMenu();
    void gotoMainPage(int id);

    void fileBackup();
    void fileExport();
    void fileSign();
    void fileVerify();
    void fileExit();

    void settingEncrypt(bool toggle);
    void settingPassphrase();
    void settingOptions();

    void helpDebugWindow();
    void helpAboutSC();
    void helpAboutQt();

public slots:
    void slotSideBarBtnClicked(int id);
    void slotGoToMainMenu();

    void slotClickedHome();
    void slotClickedFile();
    void slotClickedSettings();
    void slotClickedHelp();

    void slotFileBackup();
    void slotFileExport();
    void slotFileSign();
    void slotFileVerify();
    void slotFileExit();

    void slotSettingEncrypt();
    void slotSettingPassphrase();
    void slotSettingOptions();

    void slotHelpDebugWindow();
    void slotHelpAboutSC();
    void slotHelpAboutQt();

private:
    QStealthPage        *pages[SC_PAGE_COUNT];
    SC_PAGE_ID          pageID;
    QHoverButton        *qhbtnEncrypt;
    QHoverButton        *qhbtnChangePass;
    WalletModel         *model;

public:
    QPageOverview       *pageOverview;
    QPageSendXST        *pageSendXST;
//  QPageReceiveXST     *pageReceiveXST;
    QPageAddrBook       *pageReceiveXST;
    QPageTransactions   *pageTransactions;
    QPageAddrBook       *pageAddrBook;
    QPageIncomeExpense  *pageIncomeExpense;
//  QPageBlockExp       *pageBlockExp;
    BlockBrowser        *blockBrowser;
    QPageStatistics     *pageStatistics;
    QPageSocial         *pageSocial;

    QSideMenuBar        *sideBar;
    QTitleBar           *titleBar;

    QTitlePopup         *popupHome;
    QTitlePopup         *popupFile;
    QTitlePopup         *popupSettings;
    QTitlePopup         *popupHelp;
};

#endif // QSTEALTHMAIN_H
