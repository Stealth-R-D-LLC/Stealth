#include "qstealthsplash.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QPixmap>
#include <QBitmap>
#include <QPalette>

#define SC_SPLASH_WIDTH                 520
#define SC_SPLASH_HEIGHT                600
#define SC_SPLASH_IMAGE                 ":/resources/res/images/bg_splash.png"

#define SC_SPLASH_TIME_INTERVAL         15

#define SC_SPLASH_LOADING_LEFT          211
#define SC_SPLASH_LOADING_TOP           439


QStealthSplash::QStealthSplash(QWidget *parent) :
    // QSplashScreen(parent, QPixmap(SC_SPLASH_IMAGE), Qt::WindowStaysOnTopHint)
    QSplashScreen(parent, QPixmap(SC_SPLASH_IMAGE))
{
    setGeometry(0, 0, SC_SPLASH_WIDTH, SC_SPLASH_HEIGHT);
    QPixmap pixmap(SC_SPLASH_IMAGE);
//    setStyleSheet("background: url(" + QString(SC_SPLASH_IMAGE) + ");");
    setMask(pixmap.mask());

    QPalette palette;
    palette.setBrush(QPalette::Background, pixmap);
    setPalette(palette);

    move(QApplication::desktop()->screen()->rect().center() - this->rect().center());

    cpbLoading = new QCircleProgressBar(this);
    cpbLoading->setTransparentCenter(true);
    cpbLoading->setValue(0);
    cpbLoading->move(SC_SPLASH_LOADING_LEFT, SC_SPLASH_LOADING_TOP);
    cpbLoading->show();

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateCircleProgressBar()));
    timer->start(SC_SPLASH_TIME_INTERVAL);
}

void QStealthSplash::setPercent(int percent)
{
    cpbLoading->setValue(percent);
}

int QStealthSplash::percent()
{
    return cpbLoading->value();
}

void QStealthSplash::updateCircleProgressBar()
{
    int percent = cpbLoading->value();
    if (percent > 99) {
        timer->stop();
        emit this->loadingFinished();
        return;
    }
    cpbLoading->setValue(percent + 1);
}

