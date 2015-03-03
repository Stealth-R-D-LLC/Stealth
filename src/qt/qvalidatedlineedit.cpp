#include "qvalidatedlineedit.h"
#include "guiconstants.h"
#include "common/qstealth.h"

QValidatedLineEdit::QValidatedLineEdit(QWidget *parent) :
    QLineEdit(parent),
    valid(true),
    fHasTriAngle(false)
{
    connect(this, SIGNAL(textChanged(QString)), this, SLOT(markValid()));
    setValid(true);
}

void QValidatedLineEdit::setValid(bool valid)
{
    if (valid == this->valid) return;

    this->valid = valid;

    QString qss = SC_QSS_TOOLTIP_DEFAULT;
    qss += "QValidatedLineEdit {border: none;";
    qss += (valid) ? "background-color: #fff;" : "background-color: #f88;";
    qss += (fHasTriAngle) ? "background-image: url(:/resources/res/images/bg_triangle_right.png); background-position: left center; background-repeat: no-repeat; padding: 0px 20px; font-size: 12px;" : "";
    qss += "}";

    setStyleSheet(qss);
}

void QValidatedLineEdit::focusInEvent(QFocusEvent *evt)
{
    // Clear invalid flag on focus
    setValid(true);
    QLineEdit::focusInEvent(evt);
}

void QValidatedLineEdit::markValid()
{
    setValid(true);
}

void QValidatedLineEdit::clear()
{
    setValid(true);
    QLineEdit::clear();
}

void QValidatedLineEdit::makeTriAngle()
{
    fHasTriAngle = true;

    QString qss = SC_QSS_TOOLTIP_DEFAULT;
    qss += "QValidatedLineEdit {";
    qss += (valid) ? "background-color: #fff;" : "background-color: #f88;";
    qss += "background-image: url(:/resources/res/images/bg_triangle_right.png); background-position: left center; background-repeat: no-repeat; padding: 0px 20px; border: none; font-size: 12px;";
    qss += "}";

    setStyleSheet(qss);
}
