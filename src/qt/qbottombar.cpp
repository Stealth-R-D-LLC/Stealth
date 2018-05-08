#include "qbottombar.h"
#include "common/qstealth.h"
#include "util.h"
#include "guiutil.h"

#define SC_QSS_BOTTOM_BAR               "background-color: #000; " + SC_QSS_TOOLTIP_DEFAULT

#define SC_QSS_BOTTOM_BAR_VERSION       "color: white; font-family:" + QString(SC_BOLD_FONT_NAME) + ";"

#define SC_QSS_BTN_UNENCRYPTED          "QPushButton {background-image: url(':/resources/res/images/icons/ico_not_encrypted.png'); background-position: center; background-repeat: no-repeat; border: none; outline: none;}"
#define SC_QSS_BTN_LOCKED               "QPushButton {background-image: url(':/resources/res/images/icons/ico_lock.png'); background-position: center; background-repeat: no-repeat; border: none; outline: none;}"
#define SC_QSS_BTN_MINT_ONLY            "QPushButton {background-image: url(':/resources/res/images/icons/ico_unlock.png'); background-position: center; background-repeat: no-repeat; border: none; outline: none;}"
#define SC_QSS_BTN_UNLOCKED             "QPushButton {background-image: url(':/resources/res/images/icons/ico_unlock_red.png'); background-position: center; background-repeat: no-repeat; border: none; outline: none;}"

QStealthStatusBar::QStealthStatusBar(QWidget *parent) :
    QLabel(parent)
{
    setStyleSheet(SC_QSS_BOTTOM_BAR);

    for (int i = 0; i < SC_TRAY_ITEM_COUNT; i++) {
        lblIcons[i] = new QLabel(this);
        lblIcons[i]->setGeometry(5 + 35 * (i + 1), 10, 30, 30);
        setActive(i, true);
    }
    setActive(SC_STATUS_ID_CHECK, false);

    btnLock = new QPushButton(this);
    btnLock->setGeometry(0, 10, 30, 30);
    setLockState(SC_LS_UNLOCKED);
}

void QStealthStatusBar::setLockState(SC_LOCK_STATE nLockState)
{
    this->nLockState = nLockState;
    switch (nLockState) {
      case SC_LS_UNENCRYPTED: {
        btnLock->setStyleSheet(SC_QSS_BTN_UNENCRYPTED);
        break;
      }
      case SC_LS_UNLOCKED: {
        btnLock->setStyleSheet(SC_QSS_BTN_UNLOCKED);
        break;
      }
      case SC_LS_MINT_ONLY: {
        btnLock->setStyleSheet(SC_QSS_BTN_MINT_ONLY);
        break;
      }
      case SC_LS_LOCKED: {
        btnLock->setStyleSheet(SC_QSS_BTN_LOCKED);
        break;
      }
    }
}


SC_LOCK_STATE QStealthStatusBar::lockState()
{
    return nLockState;
}

void QStealthStatusBar::setActive(int index, bool active)
{
    // set icon
    QString iconPath[SC_TRAY_ITEM_COUNT] = {
        "ico_stealth",
        "ico_increase",
        "ico_check",
    };

    if (index < 0 || index > SC_TRAY_ITEM_COUNT - 1) return;
    state[index] = active;

    QString path = (active == false) ? SC_ICON_PATH_DEFAULT : SC_ICON_PATH_ACTIVE;
    path += iconPath[index];
    path += ".png";

    lblIcons[index]->setPixmap(QPixmap(path));
    lblIcons[index]->setObjectName("statusIcon");
    lblIcons[index]->setStyleSheet(SC_QSS_TOOLTIP_DEFAULT);
}

QBottomBar::QBottomBar(QWidget *parent) :
    QLabel(parent)
{
    setGeometry(0, 650, 1100, 50);
    setStyleSheet(SC_QSS_BOTTOM_BAR);


    QString qstrVersion = tr("stealth") +
                          "<span style=\"color:" + QString(SC_MAIN_COLOR_BLUE) +
                          ";\">" + "</span> " +
                          "<span style=\"color:" + QString(SC_MAIN_COLOR_GREY) +
                          ";\"> V " + QString::fromStdString(FormatVersionNumbers()) +
                          "</span>";

    lblVersion = new QLabel(this);
    lblVersion->setGeometry(0, 0, 250, 50);
    QFont fntVersion = GUIUtil::stealthBoldFont();
    fntVersion.setPixelSize(12);
    lblVersion->setFont(fntVersion);
    lblVersion->setText(qstrVersion);
    lblVersion->setAlignment(Qt::AlignCenter);
    lblVersion->setStyleSheet(SC_QSS_BOTTOM_BAR_VERSION);

    progressBar = new QStealthProgressBar(this);
    progressBar->setGeometry(250 + 25, 0, 310, 50);
    progressBarLabel = new QLabel(this);
    progressBarLabel->setGeometry(585, 0, 360, 50);
    progressBarLabel->setStyleSheet(QString(SC_QSS_PROGRESS_NORMAL));

    progressBarLabel->setVisible(false);

    statusBar = new QStealthStatusBar(this);
    statusBar->setGeometry(width() - 155, 0, 150, 50);

    progressBar->setPercent(0);
}
