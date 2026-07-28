// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dogebusinesspage.h"

#include "addresstablemodel.h"
#include "amount.h"
#include "dogecoinunits.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "receiverequestdialog.h"
#include "transactionrecord.h"
#include "transactiontablemodel.h"
#include "ui_interface.h"
#include "walletmodel.h"

#include "utiltime.h"

#if defined(HAVE_CONFIG_H)
#include "config/dogecoin-config.h"
#endif
#ifdef USE_QRCODE
#include <qrencode.h>
#endif

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QApplication>
#include <QClipboard>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QUrl>
#include <QUuid>
#include <QAbstractItemView>
#include <QFrame>
#include <QSettings>
#include <QModelIndex>
#include <QAbstractItemModel>
#include <QTimer>

DogeBusinessPage::DogeBusinessPage(const PlatformStyle* _platformStyle, QWidget* parent)
    : QWidget(parent),
      platformStyle(_platformStyle),
      walletModel(0),
      posBuffer(QStringLiteral("0")),
      posCurrentAmount(0),
      paymentCheckTimer(0)
{
    setupUi();
    paymentCheckTimer = new QTimer(this);
    paymentCheckTimer->setSingleShot(true);
    paymentCheckTimer->setInterval(400);
    connect(paymentCheckTimer, SIGNAL(timeout()), this, SLOT(checkIncomingPayments()));
    loadInvoices();
    rebuildInvoiceTable();
    updateDashboard();
}

void DogeBusinessPage::setupUi()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);

    QLabel* head = new QLabel(tr("Doge Business Center"));
    QFont hf = head->font();
    hf.setPointSize(hf.pointSize() + 4);
    hf.setBold(true);
    head->setFont(hf);
    root->addWidget(head);
    QLabel* sub = new QLabel(tr("Merchant tools built into Dogecoin Core — invoices, POS, and dashboard. "
                                "Your keys stay in this wallet (not external GigaWallet)."));
    sub->setWordWrap(true);
    sub->setObjectName(QStringLiteral("mutedLabel"));
    root->addWidget(sub);

    tabs = new QTabWidget(this);
    root->addWidget(tabs, 1);

    // --- Dashboard ---
    QWidget* dash = new QWidget();
    QVBoxLayout* dl = new QVBoxLayout(dash);
    QHBoxLayout* metrics = new QHBoxLayout();
    auto metric = [](const QString& title, QLabel** out) {
        QFrame* f = new QFrame();
        f->setFrameShape(QFrame::StyledPanel);
        QVBoxLayout* fl = new QVBoxLayout(f);
        QLabel* t = new QLabel(title);
        t->setObjectName(QStringLiteral("mutedLabel"));
        *out = new QLabel(QStringLiteral("—"));
        QFont bf = (*out)->font();
        bf.setBold(true);
        bf.setPointSize(bf.pointSize() + 2);
        (*out)->setFont(bf);
        fl->addWidget(t);
        fl->addWidget(*out);
        return f;
    };
    metrics->addWidget(metric(tr("Wallet balance"), &dashBalance));
    metrics->addWidget(metric(tr("Open invoices"), &dashOpen));
    metrics->addWidget(metric(tr("Paid invoices"), &dashPaid));
    metrics->addWidget(metric(tr("Invoiced volume (open+paid)"), &dashVolume));
    dl->addLayout(metrics);
    dashWatchStatus = new QLabel(tr("Auto-watch: scanning wallet for payments to open invoice addresses…"));
    dashWatchStatus->setObjectName(QStringLiteral("mutedLabel"));
    dashWatchStatus->setWordWrap(true);
    dl->addWidget(dashWatchStatus);
    QLabel* help = new QLabel(tr("Create an invoice to allocate a labeled receive address and dogecoin: payment URI. "
                                 "Open invoices auto-mark paid when this wallet receives ≥ the requested amount "
                                 "(or any amount if the invoice amount is zero). You can still Mark paid manually."));
    help->setWordWrap(true);
    dl->addWidget(help);
    QHBoxLayout* dashBtns = new QHBoxLayout();
    QPushButton* newInv = new QPushButton(tr("New invoice"));
    QPushButton* openPos = new QPushButton(tr("Open POS"));
    QPushButton* recv = new QPushButton(tr("Receive page"));
    dashBtns->addWidget(newInv);
    dashBtns->addWidget(openPos);
    dashBtns->addWidget(recv);
    dashBtns->addStretch();
    dl->addLayout(dashBtns);
    dl->addStretch();
    tabs->addTab(dash, tr("Dashboard"));
    connect(newInv, &QPushButton::clicked, [this]() { tabs->setCurrentIndex(1); });
    connect(openPos, &QPushButton::clicked, [this]() { tabs->setCurrentIndex(2); });
    connect(recv, SIGNAL(clicked()), this, SIGNAL(gotoReceiveRequested()));

    // --- Invoices ---
    QWidget* inv = new QWidget();
    QHBoxLayout* invLay = new QHBoxLayout(inv);
    QFrame* createBox = new QFrame();
    createBox->setFrameShape(QFrame::StyledPanel);
    createBox->setMaximumWidth(280);
    QVBoxLayout* cl = new QVBoxLayout(createBox);
    cl->addWidget(new QLabel(tr("Create invoice")));
    invLabel = new QLineEdit();
    invLabel->setPlaceholderText(tr("Label (customer / order #)"));
    cl->addWidget(invLabel);
    invAmount = new QDoubleSpinBox();
    invAmount->setDecimals(8);
    invAmount->setMaximum(21000000.0 * 1000);
    invAmount->setPrefix(QStringLiteral("Ð "));
    cl->addWidget(invAmount);
    invNote = new QPlainTextEdit();
    invNote->setPlaceholderText(tr("Note (optional)"));
    invNote->setMaximumHeight(80);
    cl->addWidget(invNote);
    createInvBtn = new QPushButton(tr("Create invoice"));
    cl->addWidget(createInvBtn);
    cl->addStretch();
    invLay->addWidget(createBox);

    QVBoxLayout* right = new QVBoxLayout();
    invTable = new QTableWidget(0, 5);
    invTable->setHorizontalHeaderLabels(QStringList() << tr("Created") << tr("Label") << tr("Amount")
                                                      << tr("Status") << tr("Address"));
    invTable->horizontalHeader()->setStretchLastSection(true);
    invTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    invTable->setSelectionMode(QAbstractItemView::SingleSelection);
    invTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    right->addWidget(invTable);
    QHBoxLayout* invActs = new QHBoxLayout();
    copyUriBtn = new QPushButton(tr("Copy URI"));
    copyAddrBtn = new QPushButton(tr("Copy address"));
    showQrBtn = new QPushButton(tr("Show QR"));
    showQrBtn->setToolTip(tr("Open payment request with QR code for the selected invoice"));
    markPaidBtn = new QPushButton(tr("Mark paid"));
    cancelInvBtn = new QPushButton(tr("Cancel"));
    invActs->addWidget(copyUriBtn);
    invActs->addWidget(copyAddrBtn);
    invActs->addWidget(showQrBtn);
    invActs->addWidget(markPaidBtn);
    invActs->addWidget(cancelInvBtn);
    invActs->addStretch();
    right->addLayout(invActs);
    invLay->addLayout(right, 1);
    tabs->addTab(inv, tr("Invoices"));

    connect(createInvBtn, SIGNAL(clicked()), this, SLOT(onCreateInvoice()));
    connect(copyUriBtn, SIGNAL(clicked()), this, SLOT(onCopyUri()));
    connect(copyAddrBtn, SIGNAL(clicked()), this, SLOT(onCopyAddress()));
    connect(showQrBtn, SIGNAL(clicked()), this, SLOT(onShowPaymentQr()));
    connect(markPaidBtn, SIGNAL(clicked()), this, SLOT(onMarkPaid()));
    connect(cancelInvBtn, SIGNAL(clicked()), this, SLOT(onCancelInvoice()));
    connect(invTable, SIGNAL(itemSelectionChanged()), this, SLOT(onSelectionChanged()));
    connect(invTable, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(onShowPaymentQr()));

    // --- POS ---
    QWidget* pos = new QWidget();
    QHBoxLayout* posLay = new QHBoxLayout(pos);
    QVBoxLayout* padCol = new QVBoxLayout();
    posDisplay = new QLabel(QStringLiteral("0"));
    posDisplay->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont df = posDisplay->font();
    df.setPointSize(df.pointSize() + 10);
    df.setBold(true);
    posDisplay->setFont(df);
    posDisplay->setMinimumHeight(64);
    posDisplay->setFrameShape(QFrame::StyledPanel);
    padCol->addWidget(new QLabel(tr("POS · amount (DOGE)")));
    padCol->addWidget(posDisplay);
    QGridLayout* pad = new QGridLayout();
    const char* keys[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0", "⌫"};
    for (int i = 0; i < 12; ++i) {
        QPushButton* b = new QPushButton(QString::fromUtf8(keys[i]));
        b->setMinimumSize(48, 48);
        b->setProperty("digit", QString::fromUtf8(keys[i]));
        connect(b, SIGNAL(clicked()), this, SLOT(onPosDigit()));
        pad->addWidget(b, i / 3, i % 3);
    }
    padCol->addLayout(pad);
    QHBoxLayout* posActs = new QHBoxLayout();
    QPushButton* clearBtn = new QPushButton(tr("Clear"));
    QPushButton* chargeBtn = new QPushButton(tr("Charge"));
    chargeBtn->setMinimumHeight(40);
    posActs->addWidget(clearBtn);
    posActs->addWidget(chargeBtn, 1);
    padCol->addLayout(posActs);
    padCol->addStretch();
    posLay->addLayout(padCol);

    QFrame* saleBox = new QFrame();
    saleBox->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout* sl = new QVBoxLayout(saleBox);
    sl->addWidget(new QLabel(tr("Payment request")));
    sl->addWidget(new QLabel(tr("Enter amount and press Charge — customer scans the QR.")));
    posAddress = new QLabel(tr("Address: —"));
    posAddress->setWordWrap(true);
    posAddress->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sl->addWidget(posAddress);
    posQrLabel = new QLabel();
    posQrLabel->setObjectName(QStringLiteral("posQrLabel"));
    posQrLabel->setAlignment(Qt::AlignCenter);
    posQrLabel->setMinimumSize(200, 200);
    posQrLabel->setText(tr("QR appears after Charge"));
    sl->addWidget(posQrLabel, 0, Qt::AlignHCenter);
    QHBoxLayout* saleActs = new QHBoxLayout();
    QPushButton* copyPosAddr = new QPushButton(tr("Copy address"));
    QPushButton* copyPosUri = new QPushButton(tr("Copy URI"));
    QPushButton* showPosQr = new QPushButton(tr("Open QR dialog"));
    QPushButton* newSale = new QPushButton(tr("New sale"));
    saleActs->addWidget(copyPosAddr);
    saleActs->addWidget(copyPosUri);
    saleActs->addWidget(showPosQr);
    saleActs->addWidget(newSale);
    sl->addLayout(saleActs);
    sl->addStretch();
    posLay->addWidget(saleBox, 1);
    tabs->addTab(pos, tr("POS"));

    connect(clearBtn, SIGNAL(clicked()), this, SLOT(onPosClear()));
    connect(chargeBtn, SIGNAL(clicked()), this, SLOT(onPosCharge()));
    connect(newSale, SIGNAL(clicked()), this, SLOT(onPosNewSale()));
    connect(showPosQr, SIGNAL(clicked()), this, SLOT(onPosShowQr()));
    connect(copyPosAddr, &QPushButton::clicked, [this]() {
        if (!posCurrentAddress.isEmpty())
            QApplication::clipboard()->setText(posCurrentAddress);
    });
    connect(copyPosUri, &QPushButton::clicked, [this]() {
        if (!posCurrentAddress.isEmpty())
            QApplication::clipboard()->setText(dogecoinUri(posCurrentAddress, posCurrentAmount, tr("POS sale")));
    });

    onSelectionChanged();
    updateDashboard();
    updatePosQr();
}

void DogeBusinessPage::setWalletModel(WalletModel* model)
{
    walletModel = model;
    wireWalletSignals();
    updateDashboard();
    checkIncomingPayments();
}

void DogeBusinessPage::wireWalletSignals()
{
    if (!walletModel)
        return;
    // Coalesce rapid model updates into one scan (throttle)
    connect(walletModel, SIGNAL(balanceChanged(CAmount,CAmount,CAmount,CAmount,CAmount,CAmount)),
            this, SLOT(schedulePaymentCheck()), Qt::UniqueConnection);
    TransactionTableModel* ttm = walletModel->getTransactionTableModel();
    if (ttm) {
        connect(ttm, SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(schedulePaymentCheck()), Qt::UniqueConnection);
        connect(ttm, SIGNAL(modelReset()),
                this, SLOT(schedulePaymentCheck()), Qt::UniqueConnection);
        connect(ttm, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
                this, SLOT(schedulePaymentCheck()), Qt::UniqueConnection);
    }
}

void DogeBusinessPage::schedulePaymentCheck()
{
    if (paymentCheckTimer && !paymentCheckTimer->isActive())
        paymentCheckTimer->start();
}

void DogeBusinessPage::checkIncomingPayments()
{
    if (!walletModel)
        return;
    TransactionTableModel* ttm = walletModel->getTransactionTableModel();
    if (!ttm)
        return;

    bool changed = false;
    QStringList newlyPaid;

    for (int i = 0; i < invoices.size(); ++i) {
        Invoice& inv = invoices[i];
        if (inv.status != QLatin1String("open"))
            continue;
        if (inv.address.isEmpty())
            continue;

        CAmount received = 0;
        const int rows = ttm->rowCount(QModelIndex());
        for (int row = 0; row < rows; ++row) {
            QModelIndex idx = ttm->index(row, 0, QModelIndex());
            const QString addr = ttm->data(idx, TransactionTableModel::AddressRole).toString();
            if (addr != inv.address)
                continue;
            const int type = ttm->data(idx, TransactionTableModel::TypeRole).toInt();
            if (type != TransactionRecord::RecvWithAddress && type != TransactionRecord::RecvFromOther)
                continue;
            const qint64 amt = ttm->data(idx, TransactionTableModel::AmountRole).toLongLong();
            if (amt > 0)
                received += static_cast<CAmount>(amt);
        }

        // amount==0 means “any payment”; otherwise require full invoice amount
        const bool enough = (received > 0) && (inv.amount <= 0 || received >= inv.amount);
        if (enough) {
            inv.status = QStringLiteral("paid");
            changed = true;
            newlyPaid.append(inv.label.isEmpty() ? inv.address : inv.label);
        }
    }

    if (dashWatchStatus) {
        int openN = 0;
        for (const Invoice& inv : invoices)
            if (inv.status == QLatin1String("open"))
                ++openN;
        dashWatchStatus->setText(tr("Auto-watch: %1 open invoice(s); last scan matched wallet receives.")
                                     .arg(openN));
    }

    if (!changed)
        return;

    rebuildInvoiceTable();
    updateDashboard();
    saveInvoices();

    Q_EMIT message(tr("Doge Business"),
                   tr("Payment detected — marked paid: %1").arg(newlyPaid.join(QStringLiteral(", "))),
                   CClientUIInterface::MSG_INFORMATION);
}

void DogeBusinessPage::refresh()
{
    updateDashboard();
}

void DogeBusinessPage::showTab(int index)
{
    if (!tabs)
        return;
    if (index < 0)
        index = 0;
    if (index >= tabs->count())
        index = tabs->count() - 1;
    tabs->setCurrentIndex(index);
    refresh();
}

void DogeBusinessPage::updateDashboard()
{
    int open = 0, paid = 0;
    CAmount vol = 0;
    for (const Invoice& inv : invoices) {
        if (inv.status == QLatin1String("open")) {
            ++open;
            vol += inv.amount;
        } else if (inv.status == QLatin1String("paid")) {
            ++paid;
            vol += inv.amount;
        }
    }
    dashOpen->setText(QString::number(open));
    dashPaid->setText(QString::number(paid));
    dashVolume->setText(DogecoinUnits::formatWithUnit(DogecoinUnits::BTC, vol, false, DogecoinUnits::separatorAlways));

    if (walletModel && walletModel->getOptionsModel()) {
        // Balance display optional — WalletModel has getBalance
        CAmount bal = walletModel->getBalance();
        dashBalance->setText(DogecoinUnits::formatWithUnit(
            walletModel->getOptionsModel()->getDisplayUnit(), bal, false, DogecoinUnits::separatorAlways));
    } else {
        dashBalance->setText(QStringLiteral("—"));
    }
}

QString DogeBusinessPage::allocateReceiveAddress(const QString& label)
{
    if (!walletModel)
        return QString();
    AddressTableModel* atm = walletModel->getAddressTableModel();
    if (!atm)
        return QString();
    QString addr = atm->addRow(AddressTableModel::Receive, label, QString());
    return addr;
}

QString DogeBusinessPage::dogecoinUri(const QString& address, CAmount amount, const QString& label) const
{
    QString uri = QStringLiteral("dogecoin:%1").arg(address);
    QStringList q;
    if (amount > 0)
        q << QStringLiteral("amount=%1").arg(DogecoinUnits::format(DogecoinUnits::BTC, amount, false, DogecoinUnits::separatorNever));
    if (!label.isEmpty())
        q << QStringLiteral("label=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(label)));
    if (!q.isEmpty())
        uri += QLatin1Char('?') + q.join(QLatin1Char('&'));
    return uri;
}

void DogeBusinessPage::onCreateInvoice()
{
    if (!walletModel) {
        Q_EMIT message(tr("Doge Business"), tr("Wallet not loaded."), CClientUIInterface::MSG_ERROR);
        return;
    }
    QString label = invLabel->text().trimmed();
    if (label.isEmpty())
        label = tr("Invoice");
    CAmount amount = static_cast<CAmount>(invAmount->value() * COIN + 0.5);

    QString addr = allocateReceiveAddress(label);
    if (addr.isEmpty()) {
        Q_EMIT message(tr("Doge Business"), tr("Could not allocate receive address (wallet locked?)."), CClientUIInterface::MSG_ERROR);
        return;
    }

    Invoice inv;
    inv.id = QUuid::createUuid().toString();
    inv.label = label;
    inv.amount = amount;
    inv.address = addr;
    inv.note = invNote->toPlainText().trimmed();
    inv.status = QStringLiteral("open");
    inv.created = GetTime();
    invoices.prepend(inv);

    invLabel->clear();
    invAmount->setValue(0);
    invNote->clear();
    rebuildInvoiceTable();
    updateDashboard();
    saveInvoices();
    Q_EMIT message(tr("Doge Business"), tr("Invoice created: %1").arg(addr), CClientUIInterface::MSG_INFORMATION);
}

void DogeBusinessPage::rebuildInvoiceTable()
{
    invTable->setRowCount(0);
    for (int i = 0; i < invoices.size(); ++i) {
        const Invoice& inv = invoices.at(i);
        int row = invTable->rowCount();
        invTable->insertRow(row);
        invTable->setItem(row, 0, new QTableWidgetItem(QString::number(inv.created)));
        invTable->setItem(row, 1, new QTableWidgetItem(inv.label));
        invTable->setItem(row, 2, new QTableWidgetItem(DogecoinUnits::format(DogecoinUnits::BTC, inv.amount)));
        invTable->setItem(row, 3, new QTableWidgetItem(inv.status));
        invTable->setItem(row, 4, new QTableWidgetItem(inv.address));
        invTable->item(row, 0)->setData(Qt::UserRole, inv.id);
    }
}

DogeBusinessPage::Invoice* DogeBusinessPage::selectedInvoice()
{
    QList<QTableWidgetItem*> sel = invTable->selectedItems();
    if (sel.isEmpty())
        return 0;
    int row = sel.first()->row();
    QTableWidgetItem* idItem = invTable->item(row, 0);
    if (!idItem)
        return 0;
    QString id = idItem->data(Qt::UserRole).toString();
    for (int i = 0; i < invoices.size(); ++i) {
        if (invoices[i].id == id)
            return &invoices[i];
    }
    return 0;
}

void DogeBusinessPage::onSelectionChanged()
{
    bool has = selectedInvoice() != 0;
    copyUriBtn->setEnabled(has);
    copyAddrBtn->setEnabled(has);
    showQrBtn->setEnabled(has);
    markPaidBtn->setEnabled(has);
    cancelInvBtn->setEnabled(has);
}

void DogeBusinessPage::onCopyUri()
{
    Invoice* inv = selectedInvoice();
    if (!inv)
        return;
    QApplication::clipboard()->setText(dogecoinUri(inv->address, inv->amount, inv->label));
}

void DogeBusinessPage::onCopyAddress()
{
    Invoice* inv = selectedInvoice();
    if (!inv)
        return;
    QApplication::clipboard()->setText(inv->address);
}

void DogeBusinessPage::showPaymentRequest(const QString& address, CAmount amount, const QString& label, const QString& message)
{
    if (address.isEmpty())
        return;
    SendCoinsRecipient info;
    info.address = address;
    info.label = label;
    info.amount = amount;
    info.message = message;

    ReceiveRequestDialog* dialog = new ReceiveRequestDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if (walletModel && walletModel->getOptionsModel())
        dialog->setModel(walletModel->getOptionsModel());
    dialog->setInfo(info);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->show();
}

void DogeBusinessPage::onShowPaymentQr()
{
    Invoice* inv = selectedInvoice();
    if (!inv)
        return;
    showPaymentRequest(inv->address, inv->amount, inv->label, inv->note);
}

void DogeBusinessPage::updatePosQr()
{
    if (!posQrLabel)
        return;

    if (posCurrentAddress.isEmpty()) {
        posQrLabel->setPixmap(QPixmap());
        posQrLabel->setText(tr("QR appears after Charge"));
        return;
    }

#ifdef USE_QRCODE
    const QString uri = dogecoinUri(posCurrentAddress, posCurrentAmount, tr("POS sale"));
    if (uri.length() > MAX_URI_LENGTH) {
        posQrLabel->setPixmap(QPixmap());
        posQrLabel->setText(tr("URI too long for QR"));
        return;
    }
    QRcode* code = QRcode_encodeString(uri.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (!code) {
        posQrLabel->setPixmap(QPixmap());
        posQrLabel->setText(tr("QR encode failed"));
        return;
    }
    QImage qrImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
    qrImage.fill(0xffffff);
    unsigned char* p = code->data;
    for (int y = 0; y < code->width; ++y) {
        for (int x = 0; x < code->width; ++x) {
            qrImage.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
            ++p;
        }
    }
    QRcode_free(code);

    const int size = QR_IMAGE_SIZE;
    QImage out(size, size + 18, QImage::Format_RGB32);
    out.fill(0xffffff);
    QPainter painter(&out);
    painter.drawImage(0, 0, qrImage.scaled(size, size));
    QFont font = GUIUtil::fixedPitchFont();
    font.setPixelSize(11);
    painter.setFont(font);
    QRect padded = out.rect();
    padded.setHeight(size + 14);
    painter.drawText(padded, Qt::AlignBottom | Qt::AlignCenter, posCurrentAddress.left(12) + QStringLiteral("…"));
    painter.end();
    posQrLabel->setText(QString());
    posQrLabel->setPixmap(QPixmap::fromImage(out));
#else
    posQrLabel->setPixmap(QPixmap());
    posQrLabel->setText(tr("QR not available in this build"));
#endif
}

void DogeBusinessPage::onPosShowQr()
{
    if (posCurrentAddress.isEmpty()) {
        Q_EMIT message(tr("POS"), tr("Charge a sale first."), CClientUIInterface::MSG_ERROR);
        return;
    }
    showPaymentRequest(posCurrentAddress, posCurrentAmount, tr("POS sale"), QString());
}

void DogeBusinessPage::onMarkPaid()
{
    Invoice* inv = selectedInvoice();
    if (!inv)
        return;
    inv->status = QStringLiteral("paid");
    rebuildInvoiceTable();
    updateDashboard();
    saveInvoices();
}

void DogeBusinessPage::onCancelInvoice()
{
    Invoice* inv = selectedInvoice();
    if (!inv)
        return;
    inv->status = QStringLiteral("cancelled");
    rebuildInvoiceTable();
    updateDashboard();
    saveInvoices();
}

void DogeBusinessPage::onPosDigit()
{
    QPushButton* b = qobject_cast<QPushButton*>(sender());
    if (!b)
        return;
    QString d = b->property("digit").toString();
    if (d == QString::fromUtf8("⌫")) {
        if (posBuffer.size() <= 1)
            posBuffer = QStringLiteral("0");
        else
            posBuffer.chop(1);
    } else if (d == QLatin1String(".")) {
        if (!posBuffer.contains(QLatin1Char('.')))
            posBuffer += QLatin1Char('.');
    } else {
        if (posBuffer == QLatin1String("0"))
            posBuffer = d;
        else
            posBuffer += d;
    }
    posDisplay->setText(posBuffer);
}

void DogeBusinessPage::onPosClear()
{
    posBuffer = QStringLiteral("0");
    posDisplay->setText(posBuffer);
}

void DogeBusinessPage::onPosCharge()
{
    if (!walletModel) {
        Q_EMIT message(tr("POS"), tr("Wallet not loaded."), CClientUIInterface::MSG_ERROR);
        return;
    }
    bool ok = false;
    double v = posBuffer.toDouble(&ok);
    if (!ok || v < 0) {
        Q_EMIT message(tr("POS"), tr("Invalid amount."), CClientUIInterface::MSG_ERROR);
        return;
    }
    posCurrentAmount = static_cast<CAmount>(v * COIN + 0.5);
    posCurrentAddress = allocateReceiveAddress(tr("POS %1").arg(posBuffer));
    if (posCurrentAddress.isEmpty()) {
        Q_EMIT message(tr("POS"), tr("Could not allocate receive address."), CClientUIInterface::MSG_ERROR);
        return;
    }
    posAddress->setText(tr("Address: %1\nAmount: %2 DOGE")
                            .arg(posCurrentAddress)
                            .arg(posBuffer));
    updatePosQr();

    // Also track as open invoice (auto-watch marks paid when funded)
    Invoice inv;
    inv.id = QUuid::createUuid().toString();
    inv.label = tr("POS sale");
    inv.amount = posCurrentAmount;
    inv.address = posCurrentAddress;
    inv.status = QStringLiteral("open");
    inv.created = GetTime();
    invoices.prepend(inv);
    rebuildInvoiceTable();
    updateDashboard();
    saveInvoices();
}

void DogeBusinessPage::onPosNewSale()
{
    onPosClear();
    posCurrentAddress.clear();
    posCurrentAmount = 0;
    posAddress->setText(tr("Address: —"));
    updatePosQr();
}

void DogeBusinessPage::loadInvoices()
{
    invoices.clear();
    QSettings settings;
    const int n = settings.beginReadArray(QStringLiteral("dogeBusiness/invoices"));
    for (int i = 0; i < n; ++i) {
        settings.setArrayIndex(i);
        Invoice inv;
        inv.id = settings.value(QStringLiteral("id")).toString();
        inv.label = settings.value(QStringLiteral("label")).toString();
        inv.amount = settings.value(QStringLiteral("amount")).toLongLong();
        inv.address = settings.value(QStringLiteral("address")).toString();
        inv.note = settings.value(QStringLiteral("note")).toString();
        inv.status = settings.value(QStringLiteral("status"), QStringLiteral("open")).toString();
        inv.created = settings.value(QStringLiteral("created")).toLongLong();
        if (!inv.id.isEmpty() && !inv.address.isEmpty())
            invoices.append(inv);
    }
    settings.endArray();
}

void DogeBusinessPage::saveInvoices() const
{
    QSettings settings;
    settings.beginWriteArray(QStringLiteral("dogeBusiness/invoices"), invoices.size());
    for (int i = 0; i < invoices.size(); ++i) {
        settings.setArrayIndex(i);
        const Invoice& inv = invoices.at(i);
        settings.setValue(QStringLiteral("id"), inv.id);
        settings.setValue(QStringLiteral("label"), inv.label);
        settings.setValue(QStringLiteral("amount"), static_cast<qlonglong>(inv.amount));
        settings.setValue(QStringLiteral("address"), inv.address);
        settings.setValue(QStringLiteral("note"), inv.note);
        settings.setValue(QStringLiteral("status"), inv.status);
        settings.setValue(QStringLiteral("created"), static_cast<qlonglong>(inv.created));
    }
    settings.endArray();
}
