#ifndef QHOVERBUTTON_H
#define QHOVERBUTTON_H

#include "common/qstealth.h"
#include <QPushButton>
#include <QLabel>

typedef enum {
    SC_HOVER_BUTTON_TYPE_NONE,
    SC_HOVER_BUTTON_TYPE_TEXT,
    SC_HOVER_BUTTON_TYPE_ICON
} SC_HOVER_BUTTON_TYPE;

class QHoverButton : public QPushButton
{
    Q_OBJECT
public:
    explicit QHoverButton(QWidget *parent, int id = 0);
    void setText(QString text);
    void setGeometry(QRect rect);
    void setGeometry(int x, int y, int w, int h);
    void enterEvent(QEvent *);
    void leaveEvent(QEvent *);
    void setState(SC_BUTTON_STATE state);
    void setDeactivated(bool deactivate=true);

    void setFrontStyleSheet(QString qssDefault, QString qssHover = "");
    void setBackStyleSheet(QString qssDefault, QString qssHover = "");

signals:
    void entered();
    void left();
    void clicked(int);

public slots:
    void slotClicked();

private:
    int                     id;
    bool                    deactivated;
    SC_HOVER_BUTTON_TYPE    type;
    SC_BUTTON_STATE         state;

    QLabel                  *lblFront;

    QString                 qssBackDefault;
    QString                 qssBackHover;
    QString                 qssFrontDefault;
    QString                 qssFrontHover;
};

#endif // QHOVERBUTTON_H
