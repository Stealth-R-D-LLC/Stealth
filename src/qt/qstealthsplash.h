#ifndef QSTEALTHSPLASH_H
#define QSTEALTHSPLASH_H

#include <QSplashScreen>
#include <QTimer>
#include "qcircleprogressbar.h"

class QStealthSplash : public QSplashScreen
{
    Q_OBJECT
public:
    explicit QStealthSplash(QWidget *parent = 0);
    void setPercent(int percent);
    int percent();

signals:
    void loadingFinished();

public slots:
    void updateCircleProgressBar();

private:
    QTimer             *timer;
    QCircleProgressBar *cpbLoading;
};

#endif // QSTEALTHSPLASH_H
