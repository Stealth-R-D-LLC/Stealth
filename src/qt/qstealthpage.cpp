#include "qstealthpage.h"
#include "common/qstealth.h"

QSubTitle::QSubTitle(QWidget *parent) :
    QLabel(parent)
{
    setStyleSheet("QSubTitle {background-color: #000;}");

    lblIcon = new QLabel(this);
    lblIcon->setGeometry(0, 0, 50, 50);
    lblText= new QLabel(this);
    lblText->setGeometry(50, 0, SC_PAGE_WIDTH - 50, 50);
    lblText->setStyleSheet("QLabel {background-color: none; color: " + QString(SC_MAIN_COLOR_BLUE) + "; font-size: 20px;}");
}


QStealthBlueInfo::QStealthBlueInfo(STEALTH_PAGE_ID pageID, QWidget *parent) :
    QFrame(parent)
{
    this->pageID = pageID;

    setStyleSheet("QStealthBlueInfo {background-color: #3cabe1;} QStealthBlueInfo QLabel {color: white;}");

    switch (pageID) {
    case STEALTH_PAGE_ID_NONE:
    case STEALTH_PAGE_ID_TRANSACTIONS:
    case STEALTH_PAGE_ID_BLOCK_EXPLORER:
        return;
    case STEALTH_PAGE_ID_RECEIVE_XST:
    case STEALTH_PAGE_ID_ADDRESS_BOOK:
        setGeometry(0, 0, 825, 130);
        break;
    default:
        setGeometry(0, 0, 825, 180);
        break;
    }

    QList<QRect>    arrRect;
    QList<QString>  arrText;
    QList<int>      arrFSize;
    QList<bool>     arrFBold;

    if (pageID == STEALTH_PAGE_ID_OVERVIEW ||
        pageID == STEALTH_PAGE_ID_INCOME_EXPENSE ||
        pageID == STEALTH_PAGE_ID_SEND_XST) {
        arrRect
            << QRect(50, 33, 150, 30)
            << QRect(210, 32, 200, 30)
            << QRect(50, 70, 400, 40)
            << QRect(50, 121, 220, 30)
            << QRect(270, 121, 110, 30)
            << QRect(460, 28, 130, 30)
            << QRect(460, 59, 130, 30)
            << QRect(460, 90, 130, 30)
            << QRect(460, 121, 220, 30)
            << QRect(590, 28, 200, 30)
            << QRect(590, 59, 200, 30)
            << QRect(590, 90, 200, 30)
            << QRect(590, 121, 200, 30)
        ;
        arrText
            << "NET WORTH"
            << "OUT OF SYNC"
            << "888888888.88888888 XST"
            << "Number of transactions :"
            << "888888888"
            << "Spendable :"
            << "Unconfirmed :"
            << "Stake :"
            << "Immature :"
            << "888888888.88888888 XST"
            << "888888888.88888888 XST"
            << "888888888.88888888 XST"
            << "888888888.88888888 XST"
        ;
        arrFSize << 20 << 18 << 24 << 12 << 12 << 12 << 12 << 12 << 12 << 12 << 12 << 12 << 12;
        arrFBold << 1 << 0 << 1 << 1 << 0 << 1 << 1 << 1 << 1 << 0 << 0 << 0 << 0;
    }
    else if (pageID == STEALTH_PAGE_ID_RECEIVE_XST) {
        arrRect
            << QRect(50, 28, 780, 30)
            << QRect(50, 58, 780, 30)
        ;
        arrText
            // << "These are your <strong>STEALTHCOIN | XST</strong> addresses for receiving payments."
            << "<span style=\"font-size:18px\">These are your <span style=\"font-family:" +
               QString(SC_BOLD_FONT_NAME) +
               ";\">STEALTHCOIN | XST</span> addresses for receiving payments.</span>"
            << "<span style=\"font-size:18px;font-family:Open Sans\">You want to give a different to each sender so you can keep track of who is paying you.</span>"
        ;
        arrFSize << 13 << 13;
        arrFBold << 0 << 0;
    }
    else if (pageID == STEALTH_PAGE_ID_ADDRESS_BOOK) {
        arrRect
            << QRect(50, 28, 760, 30)
        ;
        arrText
            << "Add, remove and edit all of your addresses."
        ;
        arrFSize << 13;
        arrFBold << 0;
    }
    else if (pageID == STEALTH_PAGE_ID_STATISTICS) {
        arrRect
            << QRect(50, 28, 180, 30)
            << QRect(50, 59, 180, 30)
            << QRect(50, 90, 180, 30)
            << QRect(50, 121, 180, 30)
            << QRect(225, 28, 150, 30)
            << QRect(225, 59, 150, 30)
            << QRect(225, 90, 150, 30)
            << QRect(225, 121, 150, 30)
            << QRect(460, 28, 180, 30)
            << QRect(460, 59, 180, 30)
            << QRect(460, 90, 180, 30)
            << QRect(460, 121, 180, 30)
            << QRect(640, 28, 150, 30)
            << QRect(640, 59, 150, 30)
            << QRect(640, 90, 150, 30)
            << QRect(640, 121, 150, 30)
        ;
        arrText
            << "Total supply :"
            << "Market cap :"
            << "Market cap rank :"
            << "Value :"
            << ""
            << ""
            << ""
            << ""
            << "Volume :"
            << "Volume rank :"
            << "Interest rate :"
            << ""
            << ""
            << ""
            << ""
            << ""
        ;
        arrFSize << 12 << 12 << 12 << 12 << 12 << 12 << 12 << 12
                 << 12 << 12 << 12 << 12 << 12 << 12 << 12 << 12;
        arrFBold << 1 << 1 << 1 << 1 << 0 << 0 << 0 << 0
                 << 1 << 1 << 1 << 1 << 0 << 0 << 0 << 0;
    }
    else if (pageID == STEALTH_PAGE_ID_SOCIAL) {
        arrRect
            << QRect(50, 28, 780, 30)
            << QRect(50, 58, 780, 30)
        ;
        arrText
            << "Enjoy the <strong>STEALTHCOIN | XST</strong> social network, get the latest news about your"
            << "favourite crypotcurrency and join our great community."
        ;
        arrFSize << 14 << 14;
        arrFBold << 0 << 0;
    }

    QList<int> sizes = SC_sizeFontGroups(arrFSize, arrFBold, arrRect, arrText);
    int count = arrFSize.length();
    for (int i = 0; i < count; i++) {
        QString qsFontName;
        if (arrFBold[i]) {
          qsFontName = SC_BOLD_FONT_NAME;
        } else {
          qsFontName = SC_APP_FONT_NAME;
        }
        QFont f(qsFontName, sizes[i]);
        lblInfo[i] = new QLabel(this);
        lblInfo[i]->setGeometry(arrRect[i]);
        lblInfo[i]->setFont(f);
        lblInfo[i]->setText(arrText[i]);
        lblInfo[i]->setStyleSheet("font-family:" + qsFontName + ";");
    }

    if (pageID == STEALTH_PAGE_ID_OVERVIEW ||
        pageID == STEALTH_PAGE_ID_INCOME_EXPENSE ||
        pageID == STEALTH_PAGE_ID_SEND_XST) {
        lblInfo[9]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblInfo[10]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblInfo[11]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblInfo[12]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    else if (pageID == STEALTH_PAGE_ID_STATISTICS) {
        lblInfo[4]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblInfo[5]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblInfo[6]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblInfo[7]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblInfo[12]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblInfo[13]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblInfo[14]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
}

void QStealthBlueInfo::setOverviewInfo(bool fOutOfSync, QString balance, QString spendable, QString stake, QString unconfirmed, QString immature, QString trans)
{
    if (pageID != STEALTH_PAGE_ID_OVERVIEW &&
        pageID != STEALTH_PAGE_ID_INCOME_EXPENSE &&
        pageID != STEALTH_PAGE_ID_SEND_XST) return;

    lblInfo[1]->setVisible(fOutOfSync);
    lblInfo[2]->setText(balance);
    lblInfo[4]->setText(trans);
    lblInfo[9]->setText(spendable);
    lblInfo[10]->setText(unconfirmed);
    lblInfo[11]->setText(immature);
    lblInfo[12]->setText(stake);
}


void QStealthBlueInfo::setStatisticsInfo(QString totalSupply, QString marketCap, QString marketCapRank, QString value, QString volume, QString volumeRank, QString interestRate)
{
    if (pageID != STEALTH_PAGE_ID_STATISTICS) return;

    lblInfo[4]->setText(totalSupply);
    lblInfo[5]->setText(marketCap);
    lblInfo[6]->setText(marketCapRank);
    lblInfo[7]->setText(value);
    lblInfo[12]->setText(volume);
    lblInfo[13]->setText(volumeRank);
    lblInfo[14]->setText(interestRate);
}


QStealthPage::QStealthPage(QWidget *parent) :
    QLabel(parent)
{
    setGeometry(250 + 25, 70, SC_PAGE_WIDTH, SC_PAGE_HEIGHT);
    setStyleSheet("QStealthPage {background-color: #ccc;}");

    hide();
}
