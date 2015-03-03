#include "qstealthgrid.h"
#include "common/qstealth.h"

#define SC_GRID_BTN_WIDTH               326
#define SC_GRID_BTN_HEIGHT              176
#define SC_GRID_BTN_GAP                 11
#define SC_GRID_PADDING                 50

QStealthGrid::QStealthGrid(QWidget *parent) :
    QLabel(parent)
{
    setGeometry(0, 0, SC_MAIN_WINDOW_WIDTH, SC_MAIN_WINDOW_HEIGHT);
    setStyleSheet("QStealthGrid {background-color: #333;}");

    int i, j, k = 0;

    for (j = 0; j < 3; j++) {
        for (i = 0; i < 3; i++) {
            k = j * 3 + i;
            int left = SC_GRID_PADDING + i * (SC_GRID_BTN_WIDTH + SC_GRID_BTN_GAP);
            int top = SC_GRID_PADDING + j * (SC_GRID_BTN_HEIGHT + SC_GRID_BTN_GAP);
            btn[k] = new QHoverButton(this, k);
            btn[k]->setGeometry(left, top, SC_GRID_BTN_WIDTH, SC_GRID_BTN_HEIGHT);

            switch(k) {
              case STEALTH_BTN_ID_INCOME_EXPENSE :
              case STEALTH_BTN_ID_STATISTICS :
              case STEALTH_BTN_ID_SOCIAL :
                 btn[k]->setDeactivated();
                 btn[k]->setFrontStyleSheet(QString("background-image: url(") + SC_ICON_PATH_DISABLED + SC_getIconName(k) + ".png); background-position: center; background-repeat: no-repeat;");
                 btn[k]->setBackStyleSheet(
                     QString("background-color: ") + ((k % 2 == 0) ? "#4d4d4d" : "#666"),
                     QString("background-color: ") + ((k % 2 == 0) ? "#4d4d4d" : "#666"));
                 break;
              default :
                 btn[k]->setFrontStyleSheet(QString("background-image: url(") + SC_ICON_PATH_BIGGER + SC_getIconName(k) + ".png); background-position: center; background-repeat: no-repeat;");
                 btn[k]->setBackStyleSheet(
                     QString("background-color: ") + ((k % 2 == 0) ? "#4d4d4d" : "#666"),
                     QString("background-color: ") + SC_MAIN_COLOR_BLUE);
                 connect(btn[k], SIGNAL(clicked(int)), this, SLOT(slotGridBarBtnClicked(int)));
            }
            btn[k]->setState(SC_BUTTON_STATE_DEFAULT);
        }
    }
}

void QStealthGrid::slotGridBarBtnClicked(int id)
{
    emit selectedMenuItem(id);
}
