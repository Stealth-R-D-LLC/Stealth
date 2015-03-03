#ifndef QPAGESTATISTICS_H
#define QPAGESTATISTICS_H

#include "qstealthpage.h"
#include "qhoverbutton.h"
#include <QLabel>
#include <QPaintEvent>
#include <QTextEdit>


class QGraph : public QLabel
{
    Q_OBJECT
public:
    explicit QGraph(QWidget *parent = 0);
    void paintEvent(QPaintEvent *);
};

class QPageStatistics : public QStealthPage
{
    Q_OBJECT
public:
    explicit QPageStatistics(QWidget *parent = 0);

signals:

public slots:

private:
    QStealthBlueInfo    *txtBanner;
    QSubTitle           *lblSubTitle;
};

#endif // QPAGESTATISTICS_H
