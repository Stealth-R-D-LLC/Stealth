#include "common/qstealth.h"

#include <math.h>

#include <QFile>

static QString g_slIconName[9] = {
    "ico_overview",
    "ico_send_xst",
    "ico_recv_xst",
    "ico_transactions",
    "ico_addr_book",
    "ico_income_expense",
    "ico_block_explorer",
    "ico_statistics",
    "ico_social",
};

static QString g_slTitle[9] = {
    "OVERVIEW",
    "SEND XST",
    "RECEIVE XST",
    "TRANSACTIONS",
    "ADDRESS BOOK",
    "INCOME | EXPENSE",
    "BLOCK EXPLORER",
    "STATISTICS",
    "SOCIAL",
};

QString SC_getIconName(int id)
{
    if (id < 0 || id > 8) return "";
    return g_slIconName[id];
}

QString SC_getTitle(int id)
{
    if (id < 0 || id > 8) return "";
    return g_slTitle[id];
}

int SC_sizeFont(QFont& fnt, const QRect& rect, const QString& txt) {
    int max_height = static_cast<int>(floor(rect.height() * SC_FONT_FRACTION));
    int max_width = static_cast<int>(floor(rect.width() * SC_FONT_FRACTION));
    int sz = 0;
    for (int j = SC_MIN_FONT_POINTS; j <= SC_MAX_FONT_POINTS; j++) {
       fnt.setPointSize(j);
       QFontMetrics met(fnt);
       if ((met.height() > max_height) || (met.width(txt) > max_width)) {
           fnt.setPointSize(j - 1);
           break;
       } else {
           sz = j;
       }
    }
    return sz;
}

QList<int> SC_sizeFontGroups(QList<int>& arrFSize, QList<bool>& arrFBold,
                             QList<QRect>& arrRect, QList<QString>& arrText) {

    int count = arrFSize.length();

    // keys: sizes from arrFSize; values: QList of indices for that size
    QMap<int, QList<int> > mapSizeLists;

    // keys: hints from arrFSize; values: the calculated font sizes
    QMap<int, int> mapSizes;

    int size;

    // collect the indices into the mapSizeLists
    for (int i=0; i<count; i++) {
      size = arrFSize[i];
      if (mapSizeLists.contains(size)) {
           mapSizeLists[size] << i;
      } else {
           QList<int> ql;
           ql << i;
           mapSizeLists.insert(size, ql);
      }
    }

    QFont fntApp(SC_APP_FONT_NAME);
    QFont fntBold(SC_APP_FONT_NAME);

    // find the min size for each group
    foreach(int size, mapSizeLists.keys()) {
      int minSize = SC_MAX_FONT_POINTS;
      foreach(int i, mapSizeLists[size]) {
        int thisSize;
        if (arrFBold[i]) {
          thisSize = SC_sizeFont(fntBold, arrRect[i], arrText[i]);
        } else {
          thisSize = SC_sizeFont(fntApp, arrRect[i], arrText[i]);
        }
        if (thisSize < minSize) {
             minSize = thisSize;
             // short circuit if we get a 0
             if (minSize == 0) {
                  break;
             }
        }
      }
      mapSizes[size] = minSize;
    }

    // fill the list with the measured sizes
    QList<int> sizes;
    for (int i=0; i<count; i++) {
      size = mapSizes[arrFSize[i]];
      if (size == 0) {
        size = SC_MIN_FONT_POINTS;
      }
      sizes << size;
    }

    return sizes;
}

int SC_sizeFontButton(QPushButton* button, float scale) {
  QFont fnt = button->font();
  int size = SC_sizeFont(fnt, button->geometry(), button->text());
  if ((scale != 1.0) && (size != 0)) {
    int fsize = static_cast<int>(floor(fnt.pointSize() * scale));
    fnt.setPointSize(fsize);
    size = fsize;
  }
  button->setFont(fnt);
  return size;
}

int SC_sizeFontLabel(QLabel* label, float scale) {
  QFont fnt = label->font();
  int size = SC_sizeFont(fnt, label->geometry(), label->text());
  if ((scale != 1.0) && (size != 0)) {
    int fsize = static_cast<int>(floor(fnt.pointSize() * scale));
    fnt.setPointSize(fsize);
    size = fsize;
  }
  label->setFont(fnt);
  return size;
}

int SC_sizeFontLabelGroup(QList<QLabel*> labels, float scale, bool bold) {
  int count = labels.length();
  if (count == 0) {
    return -1;
  }

  QList<int> arrFSize;
  QList<bool> arrFBold;
  QList<QRect> arrRect;
  QList<QString> arrText;

  QFont fnt = labels[0]->font();

  int size = fnt.pointSize();
  
  for (int i=0; i<count; i++) {
    arrFSize.append(size);
    fnt = labels[i]->font();
    arrFBold.append(bold);
    arrRect.append(labels[i]->geometry());
    arrText.append(labels[i]->text());
  }

  QList<int> sizes = SC_sizeFontGroups(arrFSize, arrFBold, arrRect, arrText);

  if (scale == 1.0) {
       size = sizes[0];
  } else {
       size = static_cast<int>(floor(sizes[0] * scale));
  }

  for (int i=0; i<count; i++) {
    fnt = labels[i]->font();
    fnt.setPointSize(size);
    labels[i]->setFont(fnt);
  }

  return size;
}

QString readStyleSheet(QString qsStyleSheetName) {
  QFile stylesheet(qsStyleSheetName);
  QString qsStyleSheet = "";
  bool fOpenSuccess = stylesheet.open(QFile::ReadOnly);
  if (fOpenSuccess) {
    qsStyleSheet = QLatin1String(stylesheet.readAll());
  } else {
    printf("readStyleSheet fail: %s\n", qsStyleSheetName.toStdString().c_str());
  }
  return qsStyleSheet;
}
