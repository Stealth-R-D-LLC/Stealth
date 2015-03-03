#ifndef SENDCOINSDIALOG_H
#define SENDCOINSDIALOG_H

#include "qhoverbutton.h"
#include "qstealthpage.h"
#include <QDialog>
#include <QString>
#include <QTextEdit>
#include <QLineEdit>

namespace Ui {
    class SendCoinsDialog;
}
class WalletModel;
class SendCoinsEntry;
class SendCoinsRecipient;

QT_BEGIN_NAMESPACE
class QUrl;
QT_END_NAMESPACE

/** Dialog for sending bitcoins */
class SendCoinsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SendCoinsDialog(QWidget *parent = 0);
    ~SendCoinsDialog();
    void customizeUI();

    void setModel(WalletModel *model);
    void showOutOfSyncWarning(bool fShow);

    /** Set up the tab chain manually, as Qt messes up the tab chain by default in some cases (issue https://bugreports.qt-project.org/browse/QTBUG-10907).
     */
    QWidget *setupTabChain(QWidget *prev);

    void pasteEntry(const SendCoinsRecipient &rv);
    bool handleURI(const QString &uri);

public slots:
    void clear();
    void reject();
    void accept();
    SendCoinsEntry *addEntry();
    void updateRemoveEnabled();
    void setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance);
    void setNumTransactions(int count);

    void displayBannerInfo();

private:
    Ui::SendCoinsDialog *ui;
    WalletModel         *model;
    QStealthBlueInfo    *txtBanner;
    bool fNewRecipientAllowed;
    bool fOutOfSync;
    bool fShowImmature;
    qint64 nCountOfTransactions;
    qint64 currentBalance;
    qint64 currentStake;
    qint64 currentUnconfirmedBalance;
    qint64 currentImmatureBalance;

    // added widgets for design
    QLabel              *lblLabel[3];
    QLineEdit           *edtInput[3];
    QHoverButton        *btnTransfer;
    QHoverButton        *btnSend;

private slots:
    void on_sendButton_clicked();
    void removeEntry(SendCoinsEntry* entry);
    void updateDisplayUnit();
    void coinControlFeatureChanged(bool);
    void coinControlButtonClicked();
    void coinControlChangeChecked(int);
    void coinControlChangeEdited(const QString &);
    void coinControlUpdateLabels();
    void coinControlClipboardQuantity();
    void coinControlClipboardAmount();
    void coinControlClipboardFee();
    void coinControlClipboardAfterFee();
    void coinControlClipboardBytes();
    void coinControlClipboardPriority();
    void coinControlClipboardLowOutput();
    void coinControlClipboardChange();
};

#endif // SENDCOINSDIALOG_H
