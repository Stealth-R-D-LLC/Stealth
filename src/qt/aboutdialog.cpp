#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "common/qstealth.h"
//#include "clientmodel.h"

#include "version.h"
#include "util.h"

#include <QDesktopWidget>
#include <QPixmap>
#include <QBitmap>
#include <QPalette>

#define SC_ABOUT_WIDTH                  520
#define SC_ABOUT_HEIGHT                 600
#define SC_ABOUT_IMAGE                  ":/resources/res/images/bg_about.png"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setGeometry(0, 0, SC_ABOUT_WIDTH, SC_ABOUT_HEIGHT);
    QPixmap pixmap(SC_ABOUT_IMAGE);
    setMask(pixmap.mask());

    QPalette palette;
    palette.setBrush(QPalette::Background, pixmap);
    setPalette(palette);

    QString qstrVersion = "<span style=\"font-family:" + QString(SC_BOLD_FONT_NAME) + "\">" +
                          tr("stealth") +
                          "<span style=\"color:" + QString(SC_MAIN_COLOR_BLUE) +
                          ";\">" + "</span> " +
                          "<span style=\"color:" + QString(SC_MAIN_COLOR_GREY) +
                          ";\"> V " + QString::fromStdString(FormatVersionNumbers()) +
                          "</span></span>";


    QLabel* lblVersion = new QLabel(this);
    QFont fntVersion = lblVersion->font();
    fntVersion.setPixelSize(14);
    lblVersion->setFont(fntVersion);
    lblVersion->setAlignment(Qt::AlignCenter);
    lblVersion->setGeometry(0, 0, 200, 30);
    lblVersion->move(this->rect().center() - lblVersion->rect().center());
    lblVersion->setText(qstrVersion);
    lblVersion->setStyleSheet("QLabel {color: white;}");

    QString qstrCopy = QString::fromUtf8(
      "\u00A9 2014-2018 STEALTH Developers\n"
      "This is experimental software.\n"
      "If you don't treat it as such,\n"
      "you're likely to put an eye out.");

    QLabel* lblCopy = new QLabel(this);
    QFont fntCopy = lblCopy->font();
    fntCopy.setPixelSize(12);
    lblCopy->setFont(fntCopy);
    lblCopy->setAlignment(Qt::AlignCenter);
    lblCopy->setGeometry(0, 0, 220, 70);
    QPoint pt = this->rect().center() - lblCopy->rect().center();
    lblCopy->move(pt.x(), pt.y() + 60);
    lblCopy->setText(qstrCopy);
    // lblCopy->setStyleSheet("QLabel {color: white; background-color: blue;}");
    lblCopy->setStyleSheet("QLabel {color: white;}");


    move(QApplication::desktop()->screen()->rect().center() - this->rect().center());
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::setModel(ClientModel *model)
{
    ;
}

void AboutDialog::mouseReleaseEvent(QMouseEvent *)
{
    this->accept();
}
