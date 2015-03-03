#include "qgridbutton.h"

QGridButton::QGridButton(QWidget *parent, int id) :
    QLabel(parent)
{
    this->id = id;
    this->deactivated = false;

    lblFront = new QLabel(this);
    lblFront->setGeometry(0, 0, 326, 176);

    // set icon
    QString path = SC_ICON_PATH_BIGGER;
    path += SC_getIconName(id);
    path += ".png";
    lblFront->setStyleSheet("background-image: url(" + path + "); background-position: center; background-repeat: no-repeat;");

    setStyleSheet("QGridButton:hover { qproperty-lineColor: blue; qproperty-rectColor: green; }");

    setState(SC_BUTTON_STATE_DEFAULT);
}

void QGridButton::setState(SC_BUTTON_STATE state)
{
    QString qss = "background-color: ";

    if (state == SC_BUTTON_STATE_HOVER) {
        qss += SC_MAIN_COLOR_BLUE;
    }
    else {
        qss += (id % 2 == 0) ? "#4d4d4d" : "#666";
    }

    qss += ";";
    setStyleSheet(qss);
}

void QGridButton::setDeactivated(bool deactivate) {

    //
    // Are these even used? The current buttons for the grid are
    // styled in qstealthgrid.cpp, NOT HERE!
    //

    this->deactivated = deactivate;
    // set icon

    QString path;

    printf("Setting deactivated %d.\n", this->id);

    if (this->deactivated) {
        path = SC_ICON_PATH_DISABLED;
    } else {
        path = SC_ICON_PATH_BIGGER;
    }
    path += SC_getIconName(this->id);
    path += ".png";
    this->lblFront->setStyleSheet("background-image: url(" + path + "); background-position: center; background-repeat: no-repeat;");
    this->lblFront->setStyleSheet("");
}

bool QGridButton::isDeactivated() {
       return this->deactivated;
}

void QGridButton::enterEvent(QEvent *)
{
    setState(SC_BUTTON_STATE_HOVER);
}

void QGridButton::leaveEvent(QEvent *)
{
    setState(SC_BUTTON_STATE_DEFAULT);
}

void QGridButton::mouseReleaseEvent(QMouseEvent * e)
{
    if (!rect().contains(e->pos())) return;
    emit clicked(id);
}
