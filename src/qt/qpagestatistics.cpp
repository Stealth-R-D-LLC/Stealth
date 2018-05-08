#include "qpagestatistics.h"
#include <QPainter>

#define SC_GRAPH_VIEW_WIDTH                 785
#define SC_GRAPH_VIEW_HEIGHT                300

#define SC_GRAPH_DRAW_WIDTH                 600
#define SC_GRAPH_DRAW_HEIGHT                200

#define SC_GRAPH_PADDING_LEFT               100
#define SC_GRAPH_PADDING_TOP                30

#define SC_GRAPH_X_UNIT_COUNT               6
#define SC_GRAPH_Y_UNIT_COUNT               5

#define SC_GRAPH_X_UNIT_HEIGHT              (SC_GRAPH_DRAW_WIDTH / SC_GRAPH_X_UNIT_COUNT)
#define SC_GRAPH_Y_UNIT_HEIGHT              (SC_GRAPH_DRAW_HEIGHT / SC_GRAPH_Y_UNIT_COUNT)


QGraph::QGraph(QWidget *parent) :
    QLabel(parent)
{
    ;
}

void QGraph::paintEvent(QPaintEvent *)
{
    QString yLabel[7] = {"7.0K", "6.5K", "6.0K", "5.5K", "5.0K", "4.5K"};
    QString xLabel[7] = {"-24H", "-20H", "-16H", "-12H", "-8H", "-4H", "NOW"};

    static const QPointF points[6] = {
        QPointF(100, 230),
        QPointF(110.0, 180.0),
        QPointF(120.0, 110.0),
        QPointF(180.0, 130.0),
        QPointF(190.0, 170.0),
        QPointF(700, 230),
    };

    QBrush brush = QBrush(QColor("#333"));
    QPen foreground(QColor("#333"));

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(brush);
    painter.setPen(foreground);
    painter.setFont(QFont(this->font().family(), 9));

    int left, top, w, h, i;
    left = SC_GRAPH_PADDING_LEFT;
    top = SC_GRAPH_PADDING_TOP;
    w = SC_GRAPH_DRAW_WIDTH;
    h = SC_GRAPH_DRAW_HEIGHT;

    // draw graph area
    for (i = 0; i < 5; i++) {
        painter.fillRect(left, top + i * (h / SC_GRAPH_Y_UNIT_COUNT), w, (h / SC_GRAPH_Y_UNIT_COUNT), QColor(i % 2 == 0 ? "#e6e6e6" : "#ffffff"));
    }

    // draw y - axis
    left = SC_GRAPH_PADDING_LEFT;
    top = SC_GRAPH_PADDING_TOP;
    painter.drawLine(left - 10, top, left - 10, top + h);
    painter.drawLine(left, top + h + 10, left + SC_GRAPH_DRAW_WIDTH, top + h + 10);

    for (i = 0; i < 6; i++) {
        left = SC_GRAPH_PADDING_LEFT - 10;
        top = SC_GRAPH_PADDING_TOP + i * SC_GRAPH_Y_UNIT_HEIGHT;
        painter.drawLine(left, top, left - 10, top);
        painter.drawText(QRect(left - 60, top - 10, 40, 20), Qt::AlignCenter, yLabel[i]);
    }

    for (i = 0; i < 7; i++) {
        left = SC_GRAPH_PADDING_LEFT + i * SC_GRAPH_X_UNIT_HEIGHT;
        top = SC_GRAPH_PADDING_TOP + h + 10;
        painter.drawLine(left, top, left, top + 10);
        painter.drawText(QRect(left - 20, top + 20, 40, 20), Qt::AlignCenter, xLabel[i]);
    }

    //draw graph
    painter.drawPolygon(points, 6);
}

QPageStatistics::QPageStatistics(QWidget *parent) :
    QStealthPage(parent)
{
    txtBanner = new QStealthBlueInfo(STEALTH_PAGE_ID_STATISTICS, this);

    lblSubTitle = new QSubTitle(this);
    lblSubTitle->setGeometry(0, 180, SC_PAGE_WIDTH, 50);

    QString qss = "QLabel { background-color: #000; background-image: url(";
    qss += SC_ICON_PATH_ACTIVE;
    qss += "ico_stealth";
    qss += ".png); background-repeat: no-repeat; background-position: center; }";
    lblSubTitle->lblIcon->setStyleSheet(qss);
    lblSubTitle->lblText->setText("XST VALUE IN SATOSHI");

    QGraph *graph = new QGraph(this);
    graph->setGeometry(20, 250, 785, 300);
}
