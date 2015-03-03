#include "guiutil.h"
#include "qtitlebar.h"
#include "common/qstealth.h"

QTitleBar::QTitleBar(QWidget *parent) :
    QLabel(parent)
{
    setGeometry(250 + 25, 0, 1100 - 250 - 25, 70);
    setStyleSheet("background-color: #000;");

    lblIcon = new QLabel(this);
    lblIcon->setGeometry(0, 10, 50, 50);

    lblTitle = new QLabel(this);
    lblTitle->setGeometry(50, 10, 250, 50);
    lblTitle->setFont(GUIUtil::stealthAppFont());
    lblTitle->setStyleSheet("font-size: 24px; color: " + QString(SC_MAIN_COLOR_BLUE) + ";");

    btnHome = new QHoverButton(this, 100);
    btnHome->setGeometry(1100 - 300 - 70 - 80 - 50 - 50, 10, 50, 50);
    btnFile= new QHoverButton(this, 200);
    btnFile->setFont(GUIUtil::stealthAppFont());
    btnFile->setGeometry(1100 - 300 - 70- 80 - 50, 10, 50, 50);
    btnSettings = new QHoverButton(this, 300);
    btnSettings->setFont(GUIUtil::stealthAppFont());
    btnSettings->setGeometry(1100 - 300 - 70 - 80, 10, 90, 50);
    btnHelp = new QHoverButton(this, 400);
    btnHelp->setFont(GUIUtil::stealthAppFont());
    btnHelp->setGeometry(1100 - 300 - 55, 10, 50, 50);

    btnHome->setBackStyleSheet(
        "background-image: url(" + QString(SC_ICON_PATH_DEFAULT) + "ico_home.png); background-position: center; background-repeat: no-repeat;",
        "background-image: url(" + QString(SC_ICON_PATH_ACTIVE) + "ico_home.png); background-position: center; background-repeat: no-repeat;");
    btnHome->setState(SC_BUTTON_STATE_DEFAULT);

    btnFile->setText("FILE");
    btnFile->setBackStyleSheet(
        "font-size: 13px; font-family:" + QString(SC_BOLD_FONT_NAME) +
        "; color: " + QString(SC_MAIN_COLOR_GREY) + ";",
        "font-size: 13px; font-family:" + QString(SC_BOLD_FONT_NAME) +
        "; color: " + QString(SC_MAIN_COLOR_BLUE) + ";");
    btnFile->setState(SC_BUTTON_STATE_DEFAULT);

    btnSettings->setText("SETTINGS");
    btnSettings->setBackStyleSheet(
        "font-size: 13px; font-family:" + QString(SC_BOLD_FONT_NAME) +
        "; color: " + QString(SC_MAIN_COLOR_GREY) + ";",
        "font-size: 13px; font-family:" + QString(SC_BOLD_FONT_NAME) +
        "; color: " + QString(SC_MAIN_COLOR_BLUE) + ";");
    btnSettings->setState(SC_BUTTON_STATE_DEFAULT);

    btnHelp->setText("HELP");
    btnHelp->setBackStyleSheet(
        "font-size: 13px; font-family:" + QString(SC_BOLD_FONT_NAME) +
        "; color: " + QString(SC_MAIN_COLOR_GREY) + ";",
        "font-size: 13px; font-family:" + QString(SC_BOLD_FONT_NAME) +
        "; color: " + QString(SC_MAIN_COLOR_BLUE) + ";");
    btnHelp->setState(SC_BUTTON_STATE_DEFAULT);
}

void QTitleBar::setTitleInfo(int id)
{
    QString path = SC_ICON_PATH_ACTIVE;
    path += SC_getIconName(id);
    path += ".png";
    lblIcon->setStyleSheet("background-image: url(" + path + "); background-position: center; background-repeat: no-repeat;");

    QString title = SC_getTitle(id);
    lblTitle->setText(title);
}

void QTitleBar::slotBtnHomeIsClicked()
{
    emit gotoMainMenu();
}
