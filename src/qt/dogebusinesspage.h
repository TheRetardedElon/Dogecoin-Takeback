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
class QTimer;
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
    /** 0 = Dashboard, 1 = Invoices, 2 = POS */
    void showTab(int index);

Q_SIGNALS:
    void message(const QString& title, const QString& msg, unsigned int style);
    void gotoReceiveRequested();

private Q_SLOTS:
    void onCreateInvoice();
    void onMarkPaid();
    void onCancelInvoice();
    void onCopyUri();
    void onCopyAddress();
    void onShowPaymentQr();
    void onPosDigit();
    void onPosClear();
    void onPosCharge();
    void onPosNewSale();
    void onPosShowQr();
    void onSelectionChanged();
    /** Scan wallet txs; mark open invoices paid when address is funded. */
    void checkIncomingPayments();
    void schedulePaymentCheck();

private:
    void setupUi();
    void rebuildInvoiceTable();
    void updateDashboard();
    void loadInvoices();
    void saveInvoices() const;
    void wireWalletSignals();
    void showPaymentRequest(const QString& address, CAmount amount, const QString& label, const QString& message);
    void updatePosQr();
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
    QLabel* dashWatchStatus;
    // Invoices
    QLineEdit* invLabel;
    QDoubleSpinBox* invAmount;
    QPlainTextEdit* invNote;
    QTableWidget* invTable;
    QPushButton* createInvBtn;
    QPushButton* copyUriBtn;
    QPushButton* copyAddrBtn;
    QPushButton* showQrBtn;
    QPushButton* markPaidBtn;
    QPushButton* cancelInvBtn;
    // POS
    QLabel* posDisplay;
    QLabel* posAddress;
    QLabel* posQrLabel;
    QString posBuffer;
    QString posCurrentAddress;
    CAmount posCurrentAmount;
    QTimer* paymentCheckTimer;
};

#endif // DOGECOIN_QT_DOGEBUSINESSPAGE_H
