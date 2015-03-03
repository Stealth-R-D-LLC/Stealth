#include "qsidemenubar.h"
#include "common/qstealth.h"

#define SC_SIDE_BUTTON_COUNT            9

#define SC_SIDE_BAR_WIDTH               250
#define SC_SIDE_BAR_HEIGHT              700
#define SC_SIDE_LOGO_WIDTH              250
#define SC_SIDE_LOGO_HEIGHT             200
#define SC_SIDE_MENU_WIDTH              250
#define SC_SIDE_MENU_HEIGHT             450
#define SC_SIDE_BTN_WIDTH               250
#define SC_SIDE_BTN_HEIGHT              50

#define SC_COLOR_MAIN_BACKGROUND        "#000"

#define SC_QSS_SIDE_LOGO                "background-image: url(:/resources/res/images/bg_lt_logo.png); border: none; border-width: 0px; outline: none;"
#define SC_QSS_SIDE_MENU                "background-image: url(:/resources/res/images/bg_menu.png);"


QSideMenuBar::QSideMenuBar(QWidget *parent) :
    QLabel(parent)
{
    setGeometry(0, 0, 250, 700);

    btnLogo = new QPushButton(this);
    btnLogo->setGeometry(0, 0, SC_SIDE_LOGO_WIDTH, SC_SIDE_LOGO_HEIGHT);
    btnLogo->setStyleSheet(SC_QSS_SIDE_LOGO);
    connect(btnLogo, SIGNAL(clicked()), this, SLOT(slotSideBarLogoClicked()));

    lblMenuBack = new QLabel(this);
    lblMenuBack->setGeometry(0, SC_SIDE_LOGO_HEIGHT, SC_SIDE_MENU_WIDTH, SC_SIDE_MENU_HEIGHT);
    lblMenuBack->setStyleSheet(SC_QSS_SIDE_MENU);

    int top = SC_SIDE_LOGO_HEIGHT;

    for (int i = 0; i < SC_SIDE_BUTTON_COUNT; i++) {
        btn[i] = new QSideButton(this, i);
        btn[i]->setGeometry(0, top, SC_SIDE_BTN_WIDTH, SC_SIDE_BTN_HEIGHT);
        top += SC_SIDE_BTN_HEIGHT;
        switch(i) {
          case STEALTH_BTN_ID_INCOME_EXPENSE :
          case STEALTH_BTN_ID_STATISTICS :
          case STEALTH_BTN_ID_SOCIAL :
             btn[i]->setDeactivated();
             break;
          default :
             connect(btn[i], SIGNAL(clicked(int)), this, SLOT(slotSideBarBtnClicked(int)));
        }
    }
}

void QSideMenuBar::slotSideBarLogoClicked()
{
    emit this->clickedLogoImage();
}

void QSideMenuBar::slotSideBarBtnClicked(int id)
{
    emit this->clicked(id);
}
