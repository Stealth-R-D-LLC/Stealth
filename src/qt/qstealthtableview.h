#ifndef QSTEALTHTABLEVIEW_H
#define QSTEALTHTABLEVIEW_H

#include "qhoverbutton.h"
#include <QScrollArea>
#include <QTableView>
#include <QLabel>
#include <QStyledItemDelegate>

#define SC_ROW_MAX_FIELD_COUNT          6

class QtCellItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    QtCellItemDelegate( QWidget *parent = NULL );

    void paint( QPainter *painter,
                const QStyleOptionViewItem &option,
                const QModelIndex &index ) const;

    QSize sizeHint( const QStyleOptionViewItem &option,
                    const QModelIndex & index ) const;
};

class QStealthTableHeader : public QLabel
{
    Q_OBJECT

public:
    explicit QStealthTableHeader(QWidget *parent, int fieldCount);
    void setFieldStyleSheet(int fieldIndex, QString qss);
    void setFieldWidth(int fieldIndex, int fieldWidth);
    void setFieldText(int fieldIndex, QString text);
    void setFieldSortDirection(int fieldIndex, int ascending);
    void setFieldEnable(int fieldIndex, bool enabled);
    void update();

signals:
    void clicked(int id, int sortState);

public slots:
    void slotClicked(int id);

private:
    int             fieldCount;
    int             arrFieldWidth[SC_ROW_MAX_FIELD_COUNT];
    QHoverButton    *btns[SC_ROW_MAX_FIELD_COUNT];
    QLabel          *icons[SC_ROW_MAX_FIELD_COUNT];
    int             sortState[SC_ROW_MAX_FIELD_COUNT];
};

class QStealthTableView : public QScrollArea
{
    Q_OBJECT
public:
    explicit QStealthTableView(QWidget *parent = 0);
    void setHeader(QStealthTableHeader *header);
    void hideHeader();
    void setModel(QAbstractItemModel *model);
    void update();
    void setGeometry(QRect rect);
    void setGeometry(int x, int y, int w, int h);
    void setColumnWidth(int column, int width);

signals:

public slots:
    void slotHeaderSortClicked(int id, int state);
    void slotModelRowsInserted(const QModelIndex&, int, int);

public:
    QTableView              *table;
    QStealthTableHeader     *header;

private:
    bool            fShowHeader;
    QWidget         *lblBack;
};

#endif // QSTEALTHTABLEVIEW_H
