// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_DOGEBUSINESSPAGE_H
#define DOGECOIN_QT_DOGEBUSINESSPAGE_H

#include "amount.h"

#include <QWidget>
#include <QList>
#include <QString>

class WalletModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QTabWidget;
class QTableWidget;
class QPlainTextEdit;
class QPushButton;
QT_END_NAMESPACE

/**
 * Local merchant tools inside Core (invoices + POS).
 * Keys stay in this wallet — not external GigaWallet / GPE server POS RPC.
 */
class DogeBusinessPage : public QWidget
{
    Q_OBJECT

public:
    struct Invoice {
        QString id;
        QString label;
        CAmount amount;
        QString address;
        QString note;
        QString status; // open | paid | cancelled
        qint64 created;
    };

    explicit DogeBusinessPage(const PlatformStyle* platformStyle, QWidget* parent = 0);

    void setWalletModel(WalletModel* walletModel);
    void refresh();

Q_SIGNALS:
    void message(const QString& title, const QString& msg, unsigned int style);
    void gotoReceiveRequested();

private Q_SLOTS:
    void onCreateInvoice();
    void onMarkPaid();
    void onCancelInvoice();
    void onCopyUri();
    void onCopyAddress();
    void onPosDigit();
    void onPosClear();
    void onPosCharge();
    void onPosNewSale();
    void onSelectionChanged();

private:
    void setupUi();
    void rebuildInvoiceTable();
    void updateDashboard();
    QString allocateReceiveAddress(const QString& label);
    QString dogecoinUri(const QString& address, CAmount amount, const QString& label) const;
    Invoice* selectedInvoice();

    const PlatformStyle* platformStyle;
    WalletModel* walletModel;
    QList<Invoice> invoices;

    QTabWidget* tabs;
    // Dashboard
    QLabel* dashBalance;
    QLabel* dashOpen;
    QLabel* dashPaid;
    QLabel* dashVolume;
    // Invoices
    QLineEdit* invLabel;
    QDoubleSpinBox* invAmount;
    QPlainTextEdit* invNote;
    QTableWidget* invTable;
    QPushButton* createInvBtn;
    QPushButton* copyUriBtn;
    QPushButton* copyAddrBtn;
    QPushButton* markPaidBtn;
    QPushButton* cancelInvBtn;
    // POS
    QLabel* posDisplay;
    QLabel* posAddress;
    QString posBuffer;
    QString posCurrentAddress;
    CAmount posCurrentAmount;
};

#endif // DOGECOIN_QT_DOGEBUSINESSPAGE_H
