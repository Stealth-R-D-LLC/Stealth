#ifndef STEALTH_H
#define STEALTH_H

#include <QDebug>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QFontMetrics>

#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

#define SC_LOAD_APP_QSS                 true

#define SC_QSS_TOOLTIP_DEFAULT          tr("QToolTip {color: #cecece; background-color: #333333; border: 0px; font-size: 10px; padding-top: 3px;}")

// Windsong: for testing
// #define SC_APP_FONT_PATH                ":/resources/res/fonts/Windsong/Windsong.ttf"
// #define SC_APP_FONT_NAME                "Windsong"
#define SC_APP_FONT_PATH                ":/resources/res/fonts/Open_Sans/OpenSans-Regular.ttf"
#define SC_APP_FONT_NAME                "Open Sans"
#define SC_BOLD_FONT_PATH               ":/resources/res/fonts/Open_Sans/OpenSans-Semibold.ttf"
#define SC_BOLD_FONT_NAME                "Open Sans Semibold"
#define SC_APP_FONT_SIZE                11
#define SC_APP_FONT_SPACING_TYPE        QFont::PercentageSpacing
#define SC_APP_FONT_SPACING             100

// font takes up this fraction of the total height or width of a label
#define SC_FONT_FRACTION                0.99
#define SC_MIN_FONT_POINTS              6
#define SC_MAX_FONT_POINTS              48

#define SC_TABLE_FONT_PX                13

#define SC_APP_QSS_PATH                 ":/resources/res/qss/style.qss"

#define SC_MAIN_COLOR_BLUE              "#3cabe1"
#define SC_MAIN_COLOR_WHITE             "#fff"
#define SC_MAIN_COLOR_BLACK             "#000"
#define SC_MAIN_COLOR_GREY              "#aeafb1"

#define SC_MAIN_WINDOW_WIDTH            1100
#define SC_MAIN_WINDOW_HEIGHT           700

#define SC_ICON_PATH_ACTIVE             ":/resources/res/images/icons-act/"
#define SC_ICON_PATH_DEFAULT            ":/resources/res/images/icons-def/"
#define SC_ICON_PATH_BIGGER             ":/resources/res/images/icons-big/"
#define SC_ICON_PATH_DISABLED           ":/resources/res/images/icons-dis/"

#define SC_POPUP_PATH_ACTIVE            ":/resources/res/images/popup-act/"
#define SC_POPUP_PATH_DEFAULT           ":/resources/res/images/popup-def/"
#define SC_POPUP_PATH_INACTIVE          ":/resources/res/images/popup-inact/"

#define SC_SOCIAL_PATH_ACTIVE           ":/resources/res/images/social-act/"
#define SC_SOCIAL_PATH_DEFAULT          ":/resources/res/images/social-def/"

#define SC_BTN_PATH_ACTIVE              ":/resources/res/images/btn-act/"
#define SC_BTN_PATH_DEFAULT             ":/resources/res/images/btn-def/"

#define SC_SIDEBAR_PATH_ACTIVE          ":/sidebar/res/images/sidebar_act/"
#define SC_SIDEBAR_PATH_DEFAULT         ":/sidebar/res/images/sidebar_def/"


typedef enum {
    SC_BUTTON_STATE_DEFAULT,
    SC_BUTTON_STATE_HOVER,
    SC_BUTTON_STATE_CLICKED,
    SC_BUTTON_STATE_INACTIVE
} SC_BUTTON_STATE;

typedef enum {
    STEALTH_PAGE_ID_NONE = 0,
    STEALTH_PAGE_ID_OVERVIEW,
    STEALTH_PAGE_ID_SEND_XST,
    STEALTH_PAGE_ID_RECEIVE_XST,
    STEALTH_PAGE_ID_TRANSACTIONS,
    STEALTH_PAGE_ID_ADDRESS_BOOK,
    STEALTH_PAGE_ID_INCOME_EXPENSE,
    STEALTH_PAGE_ID_BLOCK_EXPLORER,
    STEALTH_PAGE_ID_STATISTICS,
    STEALTH_PAGE_ID_SOCIAL
} STEALTH_PAGE_ID;

// same as STEALTH_PAGE_ID - 1
typedef enum {
    STEALTH_BTN_ID_OVERVIEW = 0,
    STEALTH_BTN_ID_SEND_XST,
    STEALTH_BTN_ID_RECEIVE_XST,
    STEALTH_BTN_ID_TRANSACTIONS,
    STEALTH_BTN_ID_ADDRESS_BOOK,
    STEALTH_BTN_ID_INCOME_EXPENSE,
    STEALTH_BTN_ID_BLOCK_EXPLORER,
    STEALTH_BTN_ID_STATISTICS,
    STEALTH_BTN_ID_SOCIAL
} STEALTH_BTN_ID;


QString SC_getIconName(int id);
QString SC_getTitle(int id);

// returns 0 if the rectangle is too small for SC_MIN_FONT_POINTS
// returns SC_MAX_FONT_POINTS if the rectangle is huge
// changes fnt
int SC_sizeFont(QFont& fnt, const QRect& rect, const QString& txt);

// returns a list of sizes with 1:1 correspondance to arrFSize hints
QList<int> SC_sizeFontGroups(QList<int>& arrFSize, QList<bool>& arrFBold,
                             QList<QRect>& arrRect, QList<QString>& arrText);

// adjusts button font size to its string
// returns 0 if the rectangle is too small for SC_MIN_FONT_POINTS
// returns SC_MAX_FONT_POINTS if the rectangle is huge
// after sizing, scales the font down
// changes the label's font
int SC_sizeFontButton(QPushButton* button, float scale=1.0);

// adjusts label font size to its string
// returns 0 if the rectangle is too small for SC_MIN_FONT_POINTS
// returns SC_MAX_FONT_POINTS if the rectangle is huge
// after sizing, scales the font down
// changes the label's font
int SC_sizeFontLabel(QLabel* label, float scale=1.0);

// adjusts the font sizes for the labels to the size
// returns the size selected
// returns -1 if the list is empty
// the font may fill the labels, use scale to downsize it
int SC_sizeFontLabelGroup(QList<QLabel*> labels, float scale=1.0, bool bold=false);

// reads a style sheet and returns it, return "" if fail
QString readStyleSheet(QString qsStyleSheetName);

#endif // STEALTH_H
