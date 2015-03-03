#ifndef QSTEALTHPAGE_H
#define QSTEALTHPAGE_H

#include "qhoverbutton.h"
#include "common/qstealth.h"
#include <QLabel>
#include <QListWidget>

#define SC_PAGE_WIDTH                   825
#define SC_PAGE_HEIGHT                  580

class QSubTitle : public QLabel
{
    Q_OBJECT
public:
    explicit QSubTitle(QWidget *parent = 0);

    QLabel              *lblText;
    QLabel              *lblIcon;
};


class QStealthBlueInfo : public QFrame
{
    Q_OBJECT

public:
    explicit QStealthBlueInfo(STEALTH_PAGE_ID pageID, QWidget *parent = 0);

    void setOverviewInfo(bool fOutOfSync, QString balance, QString spendable, QString stake, QString unconfirmed, QString immature, QString trans);
    void setStatisticsInfo(QString totalSupply, QString marketCap, QString marketCapRank, QString value, QString volume, QString volumeRank, QString interestRate);

private:
    STEALTH_PAGE_ID     pageID;
    QLabel *            lblInfo[20];
};


class QStealthPage : public QLabel
{
    Q_OBJECT
public:
    explicit QStealthPage(QWidget *parent = 0);

signals:

public slots:

};

#endif // QSTEALTHPAGE_H
