#ifndef QGRIDBUTTON_H
#define QGRIDBUTTON_H

#include "common/qstealth.h"
#include <QLabel>
#include <QMouseEvent>

class QGridButton : public QLabel
{
    Q_OBJECT
public:
    explicit QGridButton(QWidget *parent, int id);
    void setState(SC_BUTTON_STATE state);
    void setDeactivated(bool deactivate=true);
    bool isDeactivated();
    void enterEvent(QEvent *);
    void leaveEvent(QEvent *);
    void mouseReleaseEvent(QMouseEvent * e);

signals:
    void clicked(int id);

public slots:

private:
    int                 id;
    bool                deactivated;
    QLabel              *lblFront;
};

#endif // QGRIDBUTTON_H
