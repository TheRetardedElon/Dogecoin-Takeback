// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "memestreamrail.h"

#include "addresstablemodel.h"
#include "platformstyle.h"
#include "walletmodel.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

static const int RAIL_MAX_ITEMS = 6;

MemeStreamRail::MemeStreamRail(const PlatformStyle* _platformStyle, QWidget* parent)
    : QWidget(parent),
      platformStyle(_platformStyle),
      walletModel(0),
      client(0)
{
    setObjectName(QStringLiteral("memeStreamRail"));
    setMinimumWidth(260);
    setMaximumWidth(340);
    setupUi();

    // Defer MemeStreamClient (QNetworkAccessManager / SSL) until first refresh.
    // Eager construction + HTTPS at wallet open was tied to Windows heap corruption
    // (0xc0000374) on mingw-Qt builds.
    autoRefresh = new QTimer(this);
    autoRefresh->setInterval(60 * 1000);
    connect(autoRefresh, SIGNAL(timeout()), this, SLOT(onRefreshClicked()));
    autoRefresh->setSingleShot(false);

    // First load shortly after the home shell is painted (was "click only", which
    // looked like Meme Stream was broken). SSL/QNAM still deferred until ensureClient().
    QTimer::singleShot(1500, this, SLOT(onRefreshClicked()));
}

void MemeStreamRail::setupUi()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    QHBoxLayout* head = new QHBoxLayout();
    QLabel* title = new QLabel(tr("Meme Stream"));
    QFont tf = title->font();
    tf.setBold(true);
    tf.setPointSize(tf.pointSize() + 1);
    title->setFont(tf);
    head->addWidget(title);
    head->addStretch();
    refreshBtn = new QPushButton(tr("↻"));
    refreshBtn->setObjectName(QStringLiteral("memePrimaryButton"));
    refreshBtn->setFixedWidth(32);
    refreshBtn->setToolTip(tr("Refresh feed"));
    head->addWidget(refreshBtn);
    root->addLayout(head);

    headerStatus = new QLabel(tr("Click ↻ to load feed"));
    headerStatus->setObjectName(QStringLiteral("mutedLabel"));
    headerStatus->setWordWrap(true);
    root->addWidget(headerStatus);

    scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    container = new QWidget();
    feedLayout = new QVBoxLayout(container);
    feedLayout->setAlignment(Qt::AlignTop);
    feedLayout->setSpacing(8);
    scroll->setWidget(container);
    root->addWidget(scroll, 1);

    openFullBtn = new QPushButton(tr("Open full feed"));
    openFullBtn->setObjectName(QStringLiteral("memeOpenFullButton"));
    openFullBtn->setMinimumHeight(32);
    root->addWidget(openFullBtn);

    connect(refreshBtn, SIGNAL(clicked()), this, SLOT(onRefreshClicked()));
    connect(openFullBtn, SIGNAL(clicked()), this, SLOT(onOpenFullClicked()));
}

void MemeStreamRail::setWalletModel(WalletModel* model)
{
    walletModel = model;
    if (headerStatus && headerStatus->text().isEmpty())
        headerStatus->setText(tr("Click ↻ to load feed"));
}

void MemeStreamRail::refresh()
{
    onRefreshClicked();
}

void MemeStreamRail::ensureClient()
{
    if (client)
        return;
    client = new MemeStreamClient(this);
    connect(client, &MemeStreamClient::feedReceived, this, &MemeStreamRail::onFeedReceived);
    connect(client, &MemeStreamClient::feedFailed, this, &MemeStreamRail::onFeedFailed);
}

void MemeStreamRail::onRefreshClicked()
{
    ensureClient();
    headerStatus->setText(tr("Refreshing…"));
    client->fetchFeed(RAIL_MAX_ITEMS + 4);
    if (!autoRefresh->isActive())
        autoRefresh->start();
}

void MemeStreamRail::onOpenFullClicked()
{
    Q_EMIT openFullPage();
}

void MemeStreamRail::onFeedReceived(const QList<MemeStreamItem>& items)
{
    lastItems = items;
    headerStatus->setText(tr("%1 memes · live · tip uses author wallet address")
                               .arg(qMin(items.size(), RAIL_MAX_ITEMS)));
    rebuild(items);
}

void MemeStreamRail::onFeedFailed(const QString& error)
{
    headerStatus->setText(tr("Feed unavailable: %1").arg(error));
}

QString MemeStreamRail::currentAuthorAddress() const
{
    if (!walletModel)
        return QString();
    AddressTableModel* atm = walletModel->getAddressTableModel();
    if (!atm)
        return QString();
    for (int row = 0; row < atm->rowCount(QModelIndex()); ++row) {
        QModelIndex idx = atm->index(row, AddressTableModel::Address, QModelIndex());
        if (atm->data(idx, AddressTableModel::TypeRole).toString() == AddressTableModel::Receive) {
            QString addr = atm->data(idx, Qt::DisplayRole).toString();
            if (!addr.isEmpty())
                return addr;
        }
    }
    return QString();
}

void MemeStreamRail::rebuild(const QList<MemeStreamItem>& items)
{
    while (QLayoutItem* child = feedLayout->takeAt(0)) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }

    int n = 0;
    for (const MemeStreamItem& item : items) {
        if (n >= RAIL_MAX_ITEMS)
            break;
        feedLayout->addWidget(buildCard(item));
        ++n;
    }
    if (n == 0) {
        QLabel* empty = new QLabel(tr("No memes yet.\nOpen full feed to post from Core."));
        empty->setWordWrap(true);
        empty->setAlignment(Qt::AlignCenter);
        empty->setObjectName(QStringLiteral("mutedLabel"));
        feedLayout->addWidget(empty);
    }
    feedLayout->addStretch();
}

QWidget* MemeStreamRail::buildCard(const MemeStreamItem& item)
{
    QFrame* card = new QFrame();
    card->setObjectName(QStringLiteral("memeRailCard"));
    card->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout* lay = new QVBoxLayout(card);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(4);

    QLabel* t = new QLabel(item.title.isEmpty() ? tr("(untitled)") : item.title);
    QFont tf = t->font();
    tf.setBold(true);
    t->setFont(tf);
    t->setWordWrap(true);
    lay->addWidget(t);

    if (!item.imageUrl.isEmpty()) {
        QLabel* img = new QLabel();
        img->setObjectName(QStringLiteral("memeImage"));
        img->setMinimumHeight(80);
        img->setMaximumHeight(160);
        img->setAlignment(Qt::AlignCenter);
        lay->addWidget(img);
        if (client)
            client->loadImageInto(img, item.imageUrl, QSize(280, 150));
    }

    if (!item.body.isEmpty()) {
        QString body = item.body;
        if (body.size() > 80)
            body = body.left(77) + QStringLiteral("…");
        QLabel* b = new QLabel(body);
        b->setWordWrap(true);
        b->setObjectName(QStringLiteral("mutedLabel"));
        lay->addWidget(b);
    }

    QString tip = item.tipAddress.isEmpty() ? item.author : item.tipAddress;
    QString tipShort = tip;
    if (tipShort.size() > 14)
        tipShort = tipShort.left(6) + QStringLiteral("…") + tipShort.right(4);
    QLabel* tipLbl = new QLabel(tr("Tip → %1").arg(tipShort));
    tipLbl->setToolTip(tip);
    tipLbl->setObjectName(QStringLiteral("mutedLabel"));
    lay->addWidget(tipLbl);

    QPushButton* tipBtn = new QPushButton(tr("Tip"));
    tipBtn->setObjectName(QStringLiteral("memeTipButton"));
    tipBtn->setMinimumHeight(28);
    lay->addWidget(tipBtn);

    const QString tipAddr = tip;
    connect(tipBtn, &QPushButton::clicked, [this, tipAddr]() {
        if (!tipAddr.isEmpty())
            Q_EMIT tipRequested(tipAddr);
    });

    return card;
}
