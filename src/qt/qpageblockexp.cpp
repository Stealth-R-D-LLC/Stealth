#include "qpageblockexp.h"

QPageBlockExp::QPageBlockExp(QWidget *parent) :
    QStealthPage(parent),
    model(0)
{
    lblBottom = new QLabel(this);
    lblBottom->setGeometry(0, 320, 825, 260);
    lblBottom->setStyleSheet("background-color: #e6e6e6;");

    spbFirst = new QSpinBox(this);
    spbFirst->setGeometry(30, 30, 300, 40);
    QString qss = "QSpinBox {background-color: #fff; border: none; padding: 0px 10px; font-size: 12px;}";
    qss += "QSpinBox::up-button {padding-top: 2px; background-image: url(" + QString(SC_ICON_PATH_DEFAULT) + "bg_btn_up.png); background-position: bottom; background-repeat: no-repeat; border: none;}";
    qss += "QSpinBox::down-button {padding-bottom: 2px; background-image: url(" + QString(SC_ICON_PATH_DEFAULT) + "bg_btn_down.png); background-position: top; background-repeat: no-repeat; border: none;}";
    spbFirst->setStyleSheet(qss);
    spbFirst->setMaximum(2147483647);

    btnJump = new QHoverButton(this, 7001);
    btnJump->setGeometry(345, 30, 450, 40);
    btnJump->setFrontStyleSheet(
        "background-color: " + QString(SC_MAIN_COLOR_BLUE) + "; color: white;",
        "background-color: #4d4d4d; color: white;"
    );
    btnJump->setText("JUMP TO BLOCK");
    btnJump->setState(SC_BUTTON_STATE_DEFAULT);

    edtSecond = new QLineEdit(lblBottom);
    edtSecond->setGeometry(30, 30, 300, 40);
    edtSecond->setStyleSheet("background-color: #fff; border: none; padding: 0px 10px; font-size: 12px;");

    btnDecode = new QHoverButton(lblBottom, 7002);
    btnDecode->setGeometry(345, 30, 450, 40);
    btnDecode->setFrontStyleSheet(
        "background-color: " + QString(SC_MAIN_COLOR_BLUE) + "; color: white",
        "background-color: #4d4d4d; color: white;"
    );
    btnDecode->setText("DECODE TRANSACTION");
    btnDecode->setState(SC_BUTTON_STATE_DEFAULT);

    int i;

    QString sTopLabels[8] = {
        "Block Height:",
        "Block Hash:",
        "Block Merkle:",
        "Block nNonce:",
        "Block nBits:",
        "BlockTimestamp:",
        "Block Difficulty:",
        "Block Hashrate:"
    };

    for (i = 0; i < 8; i++) {
        lbls[i] = new QLabel(this);
        lbls[i]->setGeometry(30, 90 + i * 25, 300, 25);
        lbls[i]->setText(sTopLabels[i]);
        lbls[i]->setStyleSheet("color: #555; font-size: 15px;");
        lbls[8 + i] = new QLabel(this);
        lbls[8 + i]->setGeometry(345, 90 + i * 25, 450, 25);
        lbls[8 + i]->setText(QString().setNum(i));
        lbls[8 + i]->setStyleSheet("color: #555; font-size: 15px;");
    }

    QString sBottomLabels[5] = {
        "Transaction ID:",
        "Value out:",
        "Fees:",
        "Outputs:",
        "Inputs:"
    };

    for (i = 0; i < 5; i++) {
        lbls[16 + i] = new QLabel(lblBottom);
        lbls[16 + i]->setGeometry(30, 90 + i * 25, 300, 25);
        lbls[16 + i]->setText(sBottomLabels[i]);
        lbls[16 + i]->setStyleSheet("color: #555; font-size: 15px;");
        lbls[21 + i] = new QLabel(lblBottom);
        lbls[21 + i]->setGeometry(345, 90 + i * 25, 450, 25);
        lbls[21 + i]->setText(QString().setNum(i));
        lbls[21 + i]->setStyleSheet("color: #555; font-size: 15px;");
    }
}
