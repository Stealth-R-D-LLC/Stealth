#ifndef QTITLEBAR_H
#define QTITLEBAR_H

#include "qhoverbutton.h"
#include <QLabel>

class QTitleBar : public QLabel
{
    Q_OBJECT
public:
    explicit QTitleBar(QWidget *parent = 0);
    void setTitleInfo(int id);

signals:
    void gotoMainMenu();

public slots:
    void slotBtnHomeIsClicked();

private:
    QLabel          *lblIcon;
    QLabel          *lblTitle;

public:
    QHoverButton    *btnHome;
    QHoverButton    *btnFile;
    QHoverButton    *btnSettings;
    QHoverButton    *btnHelp;
};

#endif // QTITLEBAR_H
