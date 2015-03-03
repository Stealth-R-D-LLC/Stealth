#ifndef QTITLEPOPUP_H
#define QTITLEPOPUP_H

#include "qhoverbutton.h"
#include <QLabel>

#define SC_TITLE_POPUP_ITEM_MAX_COUNT           5

class QTitlePopup : public QLabel
{
    Q_OBJECT
public:
    explicit QTitlePopup(QLabel *parent = 0);

    void insertItem(QHoverButton *button);

signals:

public slots:
    void show();
    void hide();

private:
    int             itemCount;
    QLabel          *lblTriangle;
    QHoverButton    *btn[SC_TITLE_POPUP_ITEM_MAX_COUNT];
};

#endif // QTITLEPOPUP_H
