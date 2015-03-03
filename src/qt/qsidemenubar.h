#ifndef QSIDEMENUBAR_H
#define QSIDEMENUBAR_H

#include <QWidget>
#include "qsidebutton.h"

class QSideMenuBar : public QLabel
{
    Q_OBJECT
public:
    explicit QSideMenuBar(QWidget *parent = 0);
    QSideButton     *btn[9];

signals:
    void clicked(int);
    void clickedLogoImage();

public slots:
    void slotSideBarBtnClicked(int id);
    void slotSideBarLogoClicked();


private:
    QPushButton     *btnLogo;
    QLabel          *lblMenuBack;
};

#endif // QSIDEMENUBAR_H
