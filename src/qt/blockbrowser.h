#ifndef BLOCKBROWSER_H
#define BLOCKBROWSER_H

#include "qstealthpage.h"

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QByteArray>
#include <QTimer>

namespace Ui {
    class BlockBrowser;
}

class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Trade page widget */

class BlockBrowser : public QStealthPage
{
    Q_OBJECT

public:
    explicit BlockBrowser(QWidget *parent = 0);
    ~BlockBrowser();

    void setModel(ClientModel *clientModel);
    void setModel(WalletModel *walletModel);

public slots:

// signals:

private:
    Ui::BlockBrowser *ui;
    ClientModel *clientModel;
    WalletModel *walletModel;

private slots:

};

#endif // BLOCKBROWSER_H
