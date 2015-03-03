#include "qpagesocial.h"

QPageSocial::QPageSocial(QWidget *parent) :
    QStealthPage(parent)
{
    txtBanner = new QStealthBlueInfo(STEALTH_PAGE_ID_SOCIAL, this);

    lblSlide = new QLabel(this);
    lblSlide->setGeometry(0, 130, SC_PAGE_WIDTH, 200);
    lblSlide->setStyleSheet("background-color: " + QString(SC_MAIN_COLOR_WHITE) + ";");

    lblLRArrow = new QLabel(this);
    lblLRArrow->setGeometry(0, 330, SC_PAGE_WIDTH, 50);
    lblLRArrow->setStyleSheet("background-color: #e6e6e6;");
    btnLeft = new QHoverButton(lblLRArrow, 9010);
    btnRight = new QHoverButton(lblLRArrow, 9020);
    btnLeft->setGeometry(SC_PAGE_WIDTH / 2 - 30, 10, 30, 30);
    btnRight->setGeometry(SC_PAGE_WIDTH / 2, 10, 30, 30);
    btnLeft->setFrontStyleSheet(
        QString("background-image: url(") + SC_SOCIAL_PATH_DEFAULT + "arrow_left" + ".png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_SOCIAL_PATH_ACTIVE + "arrow_left" + ".png); background-position: center; background-repeat: no-repeat;");
    btnLeft->setState(SC_BUTTON_STATE_DEFAULT);
    btnRight->setFrontStyleSheet(
        QString("background-image: url(") + SC_SOCIAL_PATH_DEFAULT + "arrow_right" + ".png); background-position: center; background-repeat: no-repeat;",
        QString("background-image: url(") + SC_SOCIAL_PATH_ACTIVE + "arrow_right" + ".png); background-position: center; background-repeat: no-repeat;");
    btnRight->setState(SC_BUTTON_STATE_DEFAULT);

    lblSocial = new QLabel(this);
    lblSocial->setGeometry(0, 380, SC_PAGE_WIDTH, SC_PAGE_HEIGHT - 380);
    lblSocial->setStyleSheet("background-color: " + QString("none") + ";");

    QString imagePath[SC_SOCIAL_BTN_COUNT] = {
        "bg_website",
        "bg_blog",
        "bg_twitter",
        "bg_forum",
    };

    for (int i = 0; i < SC_SOCIAL_BTN_COUNT; i++) {
        btn[i] = new QHoverButton(lblSocial, 9001 + i);
        btn[i]->setGeometry(50 + 194 * i, 40, 144, 120);
        btn[i]->setFrontStyleSheet(
            QString("background-image: url(") + SC_SOCIAL_PATH_DEFAULT + imagePath[i] + ".png); background-position: center; background-repeat: no-repeat;",
            QString("background-image: url(") + SC_SOCIAL_PATH_ACTIVE + imagePath[i] + ".png); background-position: center; background-repeat: no-repeat;");
        btn[i]->setState(SC_BUTTON_STATE_DEFAULT);
    }
}

