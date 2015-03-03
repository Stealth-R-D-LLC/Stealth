#ifndef QSTEALTHGRID_H
#define QSTEALTHGRID_H

#include "qhoverbutton.h"

#define SC_GRID_BUTTON_COUNT            9

class QStealthGrid : public QLabel
{
    Q_OBJECT
public:
    explicit QStealthGrid(QWidget *parent = 0);

signals:
    void selectedMenuItem(int);

public slots:
    void slotGridBarBtnClicked(int id);

private:
    QHoverButton *btn[SC_GRID_BUTTON_COUNT];
};

#endif // QSTEALTHGRID_H
