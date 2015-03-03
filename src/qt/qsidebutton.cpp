#include "qsidebutton.h"
#include "common/qstealth.h"

#define SC_QSS_SIDE_BTN_DEFAULT             "background-color: transparent; border: none; outline: none;"
#define SC_QSS_SIDE_BTN_CLICKED             "background-color: #000; border: none; outline: none;"
#define SC_QSS_SIDE_BTN_HOVER               "background-color: #333; border: none; outline: none;"


QSideButton::QSideButton(QWidget *parent, int id) :
    QPushButton(parent)
{
    this->id = id;
    this->deactivated = false;

    // lblText = new QLabel(this);
    lblIcon = new QLabel(this);

    // lblText->setGeometry(80, 0, 170, 50);
    lblIcon->setGeometry(0, 0, 250, 50);
    // lblIcon->move(this->rect().center() - lblIcon->rect().center());

    // lblText->setText(SC_getTitle(id));


    connect(this, SIGNAL(clicked()), this, SLOT(slotClicked()));

    setState(SC_BUTTON_STATE_DEFAULT);
}

void QSideButton::setState(SC_BUTTON_STATE state)
{
    this->state = state;

    // set icon
    QString path = (state == SC_BUTTON_STATE_DEFAULT) ? SC_SIDEBAR_PATH_DEFAULT : SC_SIDEBAR_PATH_ACTIVE;
    path += SC_getIconName(id);
    path += ".png";
    lblIcon->setStyleSheet("background-image: url(" + path + "); background-position: center; background-repeat: no-repeat; border: none; outline: none;");

    // set text and color
    // QString color = (state == SC_BUTTON_STATE_DEFAULT) ? SC_MAIN_COLOR_WHITE : SC_MAIN_COLOR_BLUE;
    // lblText->setStyleSheet("background-image: url(); font-size: 14px; color: " + color + ";");

    // for button back
    if (this->deactivated) {
        setStyleSheet(SC_QSS_SIDE_BTN_DEFAULT);
    } else {
        setStyleSheet(
            state == SC_BUTTON_STATE_CLICKED ? SC_QSS_SIDE_BTN_CLICKED :
            state == SC_BUTTON_STATE_DEFAULT ? SC_QSS_SIDE_BTN_DEFAULT :  SC_QSS_SIDE_BTN_HOVER);
    }
}

void QSideButton::setDeactivated(bool deactivate) {
    this->deactivated = deactivate;
}

void QSideButton::enterEvent(QEvent *)
{
    if ((state == SC_BUTTON_STATE_CLICKED) || this->deactivated) {
       return;
    }
    setState(SC_BUTTON_STATE_HOVER);
}

void QSideButton::leaveEvent(QEvent *)
{
    if ((state == SC_BUTTON_STATE_CLICKED) || this->deactivated) {
       return;
    }
    setState(SC_BUTTON_STATE_DEFAULT);
}

void QSideButton::slotClicked()
{
    if (!this->deactivated) {
         emit clicked(id);
    }
}

//void QSideButton::mouseReleaseEvent(QMouseEvent * e)
//{
//    if (!rect().contains(e->pos())) return;
//    emit clicked(id);
//}

