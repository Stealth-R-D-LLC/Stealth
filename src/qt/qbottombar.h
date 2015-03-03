#ifndef QBOTTOMBAR_H
#define QBOTTOMBAR_H

#include "qstealthprogressbar.h"
#include <QPushButton>
#include <QLabel>

#define SC_TRAY_ITEM_COUNT                  3

#define SC_QSS_PROGRESS_NORMAL          "QLabel {background-color: #000; color: #ffffff; qproperty-alignment: AlignCenter; font-family:" + QString(SC_BOLD_FONT_NAME) + ";}"
#define SC_QSS_PROGRESS_ALERT           "QLabel {background-color: #000; color: #ff0000; qproperty-alignment: AlignCenter; font-family:" + QString(SC_BOLD_FONT_NAME) + ";}"


typedef enum {
//  SC_STATUS_ID_LOCK = 0,
    SC_STATUS_ID_MINT = 0,
    SC_STATUS_ID_CONNECTIONS,
    SC_STATUS_ID_CHECK
} SC_STATUS_ID;

typedef enum {
    SC_LS_UNENCRYPTED = 0,
    SC_LS_UNLOCKED,
    SC_LS_MINT_ONLY,
    SC_LS_LOCKED
} SC_LOCK_STATE;

class QStealthStatusBar : public QLabel
{
    Q_OBJECT
public:
    explicit QStealthStatusBar(QWidget *parent = 0);
    void setLockState(SC_LOCK_STATE nLockState);
    SC_LOCK_STATE lockState();
    void setActive(int index, bool active);

signals:

public slots:

public:
    QPushButton             *btnLock;
    SC_LOCK_STATE           nLockState;
    QLabel                  *lblIcons[SC_TRAY_ITEM_COUNT];
    bool                    state[SC_TRAY_ITEM_COUNT];
};

class QBottomBar : public QLabel
{
    Q_OBJECT
public:
    explicit QBottomBar(QWidget *parent = 0);

signals:

public slots:

public:
    QLabel                  *lblVersion;
    QStealthProgressBar     *progressBar;
    QStealthStatusBar       *statusBar;
    QLabel                  *progressBarLabel;  // for alerts etc
};

#endif // QBOTTOMBAR_H
