#include "qstealthprogressbar.h"
#include "common/qstealth.h"

#include "guiutil.h"

QStealthProgressBar::QStealthProgressBar(QWidget *parent) :
    QLabel(parent)
{
    // create & customize progress bar
    progressBar = new QProgressBar(this);
    progressBar->setGeometry(0, 20, 255, 10);
    progressBar->setRange(0, 100);
    progressBar->setTextVisible(false);
    QString qss = "QProgressBar { border: 0px solid grey; border-radius: 5px; background-color: #57575b}";
    qss += "QProgressBar::chunk { background-color: " + QString(SC_MAIN_COLOR_BLUE) + "; border-radius: 5px; }";
    progressBar->setStyleSheet(qss);

    this->setStyleSheet(SC_QSS_TOOLTIP_DEFAULT);

    // create & set style for text
    lblText = new QLabel(this);
    lblText->setFont(GUIUtil::stealthAppFont());
    lblText->setGeometry(260, 0, 50, 50);
    lblText->setStyleSheet("QLabel {color: white; qproperty-alignment: AlignCenter; font-size: 13px;}");

    setPercent(0);
}

void QStealthProgressBar::setPercent(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    progressBar->setValue(percent);
    lblText->setText(QString().setNum(percent) + "%");
    QString qss = "color:" + QString(SC_MAIN_COLOR_WHITE) +
                  "; font-family: white";
    lblText->setStyleSheet(qss);
}
