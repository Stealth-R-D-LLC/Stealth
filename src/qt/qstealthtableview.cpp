#include "qstealthtableview.h"
#include <QHeaderView>
#include <QScrollBar>
#include <QLabel>

#include "bitcoinunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"
#include <QStyledItemDelegate>
#include <QPainter>
#include <QDateTime>


#define DECORATION_SIZE 64
#define NUM_ITEMS 6

QtCellItemDelegate::QtCellItemDelegate( QWidget *parent ) :
    QStyledItemDelegate( parent )
{
    ;
}

void QtCellItemDelegate::paint(QPainter *painter,
        const QStyleOptionViewItem &option, const QModelIndex &index) const
{
#if 1
    //painter->save();
    QStyleOptionViewItem newOption(option);
    QRect rect = newOption.rect;
    int count = index.model()->columnCount();
    int column = index.column();
    int left = rect.left();
    int right = rect.right();
    int top = rect.top();
    int width = rect.width();
    int height = rect.height();

    if (column == 0) rect = QRect(left + 15, top, width, height);
    if (column == count - 1) rect = QRect(left, top, width - 15, height);
    newOption.rect = rect;
    QStyledItemDelegate::paint( painter, newOption, index );

    if (option.state & QStyle::State_Selected) {
        QColor color(SC_MAIN_COLOR_BLUE);
        if (column == 0) painter->fillRect(0, top, 15, 40, color);
        if (column == (count - 1)) painter->fillRect(right - 15, top, 16, 40, color);
    }
    //painter->restore();
#endif
}

QSize QtCellItemDelegate::sizeHint( const QStyleOptionViewItem &option,
                                     const QModelIndex & index ) const
{
    return QStyledItemDelegate::sizeHint(option, index);
}

/*
 *          QStealthTableHeader
 */

QStealthTableHeader::QStealthTableHeader(QWidget *parent, int fieldCount) :
    QLabel(parent)
{
    if (fieldCount > SC_ROW_MAX_FIELD_COUNT) fieldCount = SC_ROW_MAX_FIELD_COUNT;
    this->fieldCount = fieldCount;

    for (int i = 0; i < fieldCount; i++) {
        btns[i] = new QHoverButton(this, i);
        btns[i]->setFont(GUIUtil::stealthAppFont());
        connect(btns[i], SIGNAL(clicked(int)), this, SLOT(slotClicked(int)));
        icons[i] = new QLabel(btns[i]);
        arrFieldWidth[i] = 100;
        sortState[i] = -1;
    }

    setStyleSheet("background-color: " + QString(SC_MAIN_COLOR_BLUE) + ";");
}

void QStealthTableHeader::setFieldWidth(int fieldIndex, int fieldWidth)
{
    if (fieldIndex < 0 || fieldIndex > fieldCount - 1) return;

    arrFieldWidth[fieldIndex] = fieldWidth;
}

void QStealthTableHeader::setFieldStyleSheet(int fieldIndex, QString qss)
{
    if (fieldIndex < 0 || fieldIndex > fieldCount - 1) return;
    if (qss == "") return;

    btns[fieldIndex]->setFont(GUIUtil::stealthAppFont());
    btns[fieldIndex]->setStyleSheet(qss);
}

void QStealthTableHeader::setFieldText(int fieldIndex, QString text)
{
    if (fieldIndex < 0 || fieldIndex > fieldCount - 1) return;

    btns[fieldIndex]->setText(text);
}

void QStealthTableHeader::setFieldSortDirection(int fieldIndex, int ascending)
{
    if (fieldIndex < 0 || fieldIndex > fieldCount - 1) return;

    QString imageName[3] = {
        "",
        "ico_tri_down",
        "ico_tri_up",
    };

    sortState[fieldIndex] = ascending;

    QString qss;
    qss = "QLabel {background-image: url(";
    qss += SC_ICON_PATH_DEFAULT;
    qss += imageName[ascending + 1];
    qss += ".png);";
    qss += "background-position: center; background-repeat: no-repeat;}";

    icons[fieldIndex]->setStyleSheet(qss);
}

void QStealthTableHeader::setFieldEnable(int fieldIndex, bool enable)
{
    if (fieldIndex < 0 || fieldIndex > fieldCount - 1) return;

    btns[fieldIndex]->setEnabled(enable);
}

void QStealthTableHeader::slotClicked(int id)
{
    if (id < 0 || id > fieldCount - 1) return;

    sortState[id] = (sortState[id] == 1) ? 0 : 1;

    setFieldSortDirection(id, sortState[id]);

    emit clicked(id, sortState[id]);
}

void QStealthTableHeader::update()
{
    int i, left = 0, vw = 0, width = this->width() - 17;
    for (i = 0; i < fieldCount; i++) {
        vw += arrFieldWidth[i];
    }
    for (i = 0; i < fieldCount; i++) {
        int w = arrFieldWidth[i] * width / vw + 1;
        btns[i]->setGeometry(left, 0, w, 50);
        icons[i]->setGeometry(w - 50, 0, 50, 50);
        left += w;
    }
}

QStealthTableView::QStealthTableView(QWidget *parent) :
    QScrollArea(parent),
    header(0),
    fShowHeader(true)
{
    this->horizontalScrollBar()->setDisabled(true);
    this->horizontalScrollBar()->hide();
    this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lblBack = new QLabel(this);
    lblBack->setStyleSheet("QLabel {background-color: #ccc; border: none;}");
    table = new QTableView(lblBack);
    this->setWidget(lblBack);

    QtCellItemDelegate *delegate = new QtCellItemDelegate(this);
    table->setItemDelegate(delegate);

    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->hide();
    table->verticalHeader()->hide();
    table->setAlternatingRowColors(true);
    table->setAutoScroll(false);
    table->setShowGrid(false);
    table->setWordWrap(false);
    table->setCornerButtonEnabled(false);
    table->setFocusPolicy(Qt::NoFocus);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setDefaultSectionSize(40);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->horizontalScrollBar()->setDisabled(true);
    table->verticalScrollBar()->setDisabled(true);

    QString qss = SC_QSS_TOOLTIP_DEFAULT;
    qss += "QTableView {   border: none;"
                         " selection-color: #fff;"
                         " selection-background-color: #3cabe1;"
                         " background-color: #fff;"
                         " font-size: " + QString(TO_STRING(SC_TABLE_FONT_PX)) + "px;"
                         " alternate-background-color: #e8e8e8; }";

    table->setStyleSheet(qss);
#if 1
    this->setStyleSheet("border: none; background-color: #ccc;");

    qss = "";
    qss += "QScrollBar::handle:vertical{background-color: #3e3e41; min-height:40px;}";
    qss += "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical{background-color: #999;}";
    this->verticalScrollBar()->setStyleSheet(qss);

    this->lblBack->setStyleSheet("QLineEdit {background-color: white; border: 1px solid #999;}");
#else
    qss = "QStealthTableView {border: none; background-color: #ccc;}";
    this->setStyleSheet(qss);

    qss = "";
    qss += "QScrollBar::handle:vertical{background-color: #3e3e41; min-height:40px;}";
    qss += "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical{background-color: #999;}";
    this->verticalScrollBar()->setStyleSheet(qss);
#endif
}

void QStealthTableView::setHeader(QStealthTableHeader *header)
{
    this->header = header;
    connect(header, SIGNAL(clicked(int,int)), this, SLOT(slotHeaderSortClicked(int,int)));
}

void QStealthTableView::hideHeader()
{
    fShowHeader = false;
}

void QStealthTableView::setModel(QAbstractItemModel *model)
{
    table->setModel(model);
    connect(model, SIGNAL(rowsInserted(const QModelIndex&, int, int)), this, SLOT(slotModelRowsInserted(const QModelIndex&, int, int)));
    update();
}

void QStealthTableView::update()
{
    if (table == NULL) return;
    if (table->model() == NULL) return;

    int count = table->model()->rowCount();
    int padding = 15;
    int top = (fShowHeader ? 50 : 0);
    int w = this->width();
    int h = 40 * count + padding * 2;
    if (h > height() - top || this->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOn) w = w - 17;

    lblBack->setGeometry(0, 0, w, top + h);
    table->setGeometry(padding, top + padding, w - padding * 2, h - padding * 2);
    if (header == NULL) return;
    header->setGeometry(0, 0, w, 50);
    header->update();
}

void QStealthTableView::setGeometry(QRect rect)
{
    QScrollArea::setGeometry(rect);

    lblBack->setGeometry(0, 0, rect.width(), 0);

    int padding = 15;
    int w = rect.width() - padding * 2;

    table->setGeometry(padding, padding, w, 0);
}

void QStealthTableView::setGeometry(int x, int y, int w, int h)
{
    this->setGeometry(QRect(x, y, w, h));
}

void QStealthTableView::setColumnWidth(int column, int width)
{
    table->setColumnWidth(column, width);
}

void QStealthTableView::slotHeaderSortClicked(int id, int state)
{
    if (state < 0) return;
    table->sortByColumn(id, (state > 0) ? Qt::AscendingOrder : Qt::DescendingOrder);
}

void QStealthTableView::slotModelRowsInserted(const QModelIndex&, int, int)
{
    update();
    // not really sure why the scroll needs to change upon a new transaction
    // this->verticalScrollBar()->setValue(this->verticalScrollBar()->minimum());
}
