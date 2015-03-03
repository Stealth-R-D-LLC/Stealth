#ifndef QSIDEBUTTON_H
#define QSIDEBUTTON_H

#include "common/qstealth.h"
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>

class QSideButton : public QPushButton
{
    Q_OBJECT
public:
    explicit QSideButton(QWidget *parent, int id);
    void setDeactivated(bool deactivate=true);
    void setState(SC_BUTTON_STATE state);
    void enterEvent(QEvent *);
    void leaveEvent(QEvent *);

signals:
    void clicked(int id);

public slots:
    void slotClicked();

private:
    int             id;
    int             state;
    int             deactivated;
    QLabel          *lblBack;
    QLabel          *lblIcon;
    QLabel          *lblText;
};

#endif // QSIDEBUTTON_H
