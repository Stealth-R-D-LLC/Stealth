#ifndef QSTEALTHPROGRESSBAR_H
#define QSTEALTHPROGRESSBAR_H

#include <QLabel>
#include <QProgressBar>

class QStealthProgressBar : public QLabel
{
    Q_OBJECT
public:
    explicit QStealthProgressBar(QWidget *parent = 0);

public slots:
    void setPercent(int percent);

private:
    QProgressBar        *progressBar;
    QLabel              *lblText;
};

#endif // QSTEALTHPROGRESSBAR_H
