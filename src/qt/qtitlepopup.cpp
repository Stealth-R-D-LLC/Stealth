#include "qtitlepopup.h"

QTitlePopup::QTitlePopup(QLabel *parent) :
    QLabel(parent)
{
    itemCount = 0;
    for (int i = 0; i < SC_TITLE_POPUP_ITEM_MAX_COUNT; i++) {
        btn[i] = NULL;
    }
    setStyleSheet("QTitlePopup {background-color: transparent;}");

    lblTriangle = new QLabel(this);
    lblTriangle->setGeometry(0, 0, 200, 9);
    lblTriangle->setStyleSheet("QLabel {background-image: url(:/resources/res/images/bg_triangle.png); background-position: right center; background-repeat: no-repeat;}");

    hide();
}

void QTitlePopup::insertItem(QHoverButton *button)
{
    if (button == NULL) return;
    if (itemCount < 0) return;
    if (itemCount > SC_TITLE_POPUP_ITEM_MAX_COUNT - 1) return;

    // resize button
    button->setGeometry(0, 9 + itemCount * 30, 200, 30);

    btn[itemCount] = button;
    connect(btn[itemCount], SIGNAL(clicked()), this, SLOT(hide()));
    itemCount++;

    setMinimumHeight(9 + 30 * itemCount);
}

void QTitlePopup::show()
{
    for (int i = 0; i < itemCount; i++) {
        btn[i]->setState(SC_BUTTON_STATE_DEFAULT);
    }

    QLabel::show();
}

void QTitlePopup::hide()
{
    QLabel::hide();
}
