#include "bitcoinamountfield.h"
#include "qvaluecombobox.h"
#include "bitcoinunits.h"

#include "guiconstants.h"
#include "common/qstealth.h"

#include <QMessageBox>
#include <QLabel>
#include <QLineEdit>
#include <QRegExpValidator>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QApplication>
#include <qmath.h>

BitcoinAmountField::BitcoinAmountField(QWidget *parent):
        QWidget(parent),
        amount(0),
        currentUnit(-1),
        fHasTriAngle(false)
{
    amount = new QDoubleSpinBox(this);
    amount->setLocale(QLocale::c());
    amount->setDecimals(8);
    amount->installEventFilter(this);
    amount->setSingleStep(0.001);

    unit = new QValueComboBox(this);
    unit->setModel(new BitcoinUnits(this));

    if (parent && parent->parent() && parent->parent()->inherits("SendCoinsEntry")) {
        amount->setFixedSize(400, 30);
	    ((QDoubleSpinBoxEx *)amount)->lineEdit()->setPlaceholderText("0.000000");
	    unit->hide();
    }
	else {
	    amount->setMaximumWidth(170);
	    QHBoxLayout *layout = new QHBoxLayout(this);
	    layout->addWidget(amount);
	    layout->addWidget(unit);
	    layout->addStretch(1);
	    layout->setContentsMargins(0,0,0,0);

	    setLayout(layout);
	}

    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(amount);

    // If one if the widgets changes, the combined content changes as well
    connect(amount, SIGNAL(valueChanged(QString)), this, SIGNAL(textChanged()));
    connect(unit, SIGNAL(currentIndexChanged(int)), this, SLOT(unitChanged(int)));

    // Set default based on configuration
    unitChanged(unit->currentIndex());
}

void BitcoinAmountField::setText(const QString &text)
{
    if (text.isEmpty())
        amount->clear();
    else
        amount->setValue(text.toDouble());
}

void BitcoinAmountField::clear()
{
    amount->clear();
    unit->setCurrentIndex(0);
}

bool BitcoinAmountField::validate()
{
    bool valid = true;
    if (amount->value() == 0.0)
        valid = false;
    if (valid && !BitcoinUnits::parse(currentUnit, text(), 0))
        valid = false;

    setValid(valid);

    return valid;
}

void BitcoinAmountField::setValid(bool valid)
{
    QWidget* parent = (QWidget*)this->parent();
    if (parent && parent->parent() && parent->parent()->inherits("SendCoinsEntry")) {
        ;
    }
    else {
        amount->setStyleSheet((valid) ? "" : "background-color: #f88;");
        return;
    }
    QString qss = SC_QSS_TOOLTIP_DEFAULT;
    qss += "QDoubleSpinBox {border: none;";
    qss += (valid) ? "background-color: #fff;" : "background-color: #f88;";
    qss += (fHasTriAngle) ? "background-image: url(:/resources/res/images/bg_triangle_right.png); background-position: left center; background-repeat: no-repeat; padding: 0px 20px; font-size: 12px;" : "";
    qss += "}";
    qss += "QDoubleSpinBox::up-button {width: 15px; margin: 2px 5px 0 0; border-image: url(" + QString(SC_ICON_PATH_DEFAULT) + "bg_btn_up.png); border: none;}";
    qss += "QDoubleSpinBox::down-button {width: 15px; margin: 0 5px 2px 0; border-image: url(" + QString(SC_ICON_PATH_DEFAULT) + "bg_btn_down.png); border: none;}";

    amount->setStyleSheet(qss);
}

QString BitcoinAmountField::text() const
{
    if (amount->text().isEmpty())
        return QString();
    else
        return amount->text();
}

bool BitcoinAmountField::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusIn)
    {
        // Clear invalid flag on focus
        setValid(true);
    }
    else if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Comma)
        {
            // Translate a comma into a period
            QKeyEvent periodKeyEvent(event->type(), Qt::Key_Period, keyEvent->modifiers(), ".", keyEvent->isAutoRepeat(), keyEvent->count());
            qApp->sendEvent(object, &periodKeyEvent);
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

QWidget *BitcoinAmountField::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, amount);
    return amount;
}

qint64 BitcoinAmountField::value(bool *valid_out) const
{
    qint64 val_out = 0;
    bool valid = BitcoinUnits::parse(currentUnit, text(), &val_out);
    if(valid_out)
    {
        *valid_out = valid;
    }
    return val_out;
}

void BitcoinAmountField::setValue(qint64 value)
{
    setText(BitcoinUnits::format(currentUnit, value));
}

void BitcoinAmountField::unitChanged(int idx)
{
    // Use description tooltip for current unit for the combobox
    unit->setToolTip(unit->itemData(idx, Qt::ToolTipRole).toString());

    // Determine new unit ID
    int newUnit = unit->itemData(idx, BitcoinUnits::UnitRole).toInt();

    // Parse current value and convert to new unit
    bool valid = false;
    qint64 currentValue = value(&valid);

    currentUnit = newUnit;

    // Set max length after retrieving the value, to prevent truncation
    amount->setDecimals(BitcoinUnits::decimals(currentUnit));
    amount->setMaximum(qPow(10, BitcoinUnits::amountDigits(currentUnit)) - qPow(10, -amount->decimals()));

    if(valid)
    {
        // If value was valid, re-place it in the widget with the new unit
        setValue(currentValue);
    }
    else
    {
        // If current value is invalid, just clear field
        setText("");
    }
    setValid(true);
}

void BitcoinAmountField::setDisplayUnit(int newUnit)
{
    unit->setValue(newUnit);
}

void BitcoinAmountField::makeTriAngle()
{
    fHasTriAngle = true;

    QString qss = SC_QSS_TOOLTIP_DEFAULT;
    qss += "QDoubleSpinBox {height: 30px;border: none;";
    qss += "background-color: #fff;";
    qss += "background-image: url(:/resources/res/images/bg_triangle_right.png); background-position: left center; background-repeat: no-repeat; padding: 0px 20px; font-size: 12px;";
    qss += "}";
    qss += "QDoubleSpinBox::up-button {width: 15px; margin: 2px 5px 0 0; border-image: url(" + QString(SC_ICON_PATH_DEFAULT) + "bg_btn_up.png); border: none;}";
    qss += "QDoubleSpinBox::down-button {width: 15px; margin: 0 5px 2px 0; border-image: url(" + QString(SC_ICON_PATH_DEFAULT) + "bg_btn_down.png); border: none;}";

    amount->setStyleSheet(qss);
}
