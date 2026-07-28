// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "memestreampage.h"

#include "addresstablemodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "platformstyle.h"
#include "ui_interface.h"
#include "walletmodel.h"

#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

static const int MAX_IMAGE_BYTES = 70656; // 69 KiB

MemeStreamPage::MemeStreamPage(const PlatformStyle* _platformStyle, QWidget* parent)
    : QWidget(parent),
      platformStyle(_platformStyle),
      walletModel(0),
      client(new MemeStreamClient(this)),
      pendingImage(),
      pendingImageName()
{
    setupUi();
    connect(client, &MemeStreamClient::feedReceived, this, &MemeStreamPage::onFeedReceived);
    connect(client, &MemeStreamClient::feedFailed, this, &MemeStreamPage::onFeedFailed);
    connect(client, &MemeStreamClient::publishSucceeded, this, &MemeStreamPage::onPublishSucceeded);
    connect(client, &MemeStreamClient::publishFailed, this, &MemeStreamPage::onPublishFailed);
    connect(client, &MemeStreamClient::likeSucceeded, this, &MemeStreamPage::onLikeSucceeded);
    connect(client, &MemeStreamClient::likeFailed, this, &MemeStreamPage::onLikeFailed);
}

MemeStreamPage::~MemeStreamPage()
{
}

void MemeStreamPage::setupUi()
{
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    QHBoxLayout* header = new QHBoxLayout();
    QLabel* title = new QLabel(tr("Meme Stream"));
    QFont tf = title->font();
    tf.setPointSize(tf.pointSize() + 4);
    tf.setBold(true);
    title->setFont(tf);
    header->addWidget(title);
    header->addStretch();
    refreshBtn = new QPushButton(tr("Refresh"));
    webBtn = new QPushButton(tr("View on web"));
    header->addWidget(refreshBtn);
    header->addWidget(webBtn);
    root->addLayout(header);

    // Publish card
    QFrame* publishFrame = new QFrame(this);
    publishFrame->setObjectName("memePublishFrame");
    publishFrame->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout* pub = new QVBoxLayout(publishFrame);
    QLabel* pubHead = new QLabel(tr("Post a meme (Dogecoin Core only)"));
    pubHead->setStyleSheet("font-weight: bold;");
    pub->addWidget(pubHead);
    authorLabel = new QLabel(tr("Author (wallet address — used for Tip): —"));
    authorLabel->setWordWrap(true);
    pub->addWidget(authorLabel);
    titleEdit = new QLineEdit(this);
    titleEdit->setPlaceholderText(tr("Title"));
    pub->addWidget(titleEdit);
    bodyEdit = new QPlainTextEdit(this);
    bodyEdit->setPlaceholderText(tr("Body / caption"));
    bodyEdit->setMaximumHeight(90);
    pub->addWidget(bodyEdit);
    QHBoxLayout* imgRow = new QHBoxLayout();
    chooseImageBtn = new QPushButton(tr("Choose image…"));
    imageLabel = new QLabel(tr("No image (optional, ≤ 69 KiB)"));
    imgRow->addWidget(chooseImageBtn);
    imgRow->addWidget(imageLabel, 1);
    pub->addLayout(imgRow);
    publishBtn = new QPushButton(tr("Publish to Meme Stream"));
    publishBtn->setMinimumHeight(36);
    pub->addWidget(publishBtn);
    QLabel* hint = new QLabel(tr("Your Dogecoin receive address is set as author so others can Tip you. "
                                 "Posts use the built-in Core publish key (web remains view-only)."));
    hint->setWordWrap(true);
    hint->setStyleSheet("color: gray; font-size: 11px;");
    pub->addWidget(hint);
    root->addWidget(publishFrame);

    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    root->addWidget(statusLabel);

    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(tr("Search memes…"));
    root->addWidget(searchEdit);

    feedScroll = new QScrollArea(this);
    feedScroll->setWidgetResizable(true);
    feedScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    feedContainer = new QWidget();
    feedLayout = new QVBoxLayout(feedContainer);
    feedLayout->setAlignment(Qt::AlignTop);
    feedLayout->addStretch();
    feedScroll->setWidget(feedContainer);
    root->addWidget(feedScroll, 1);

    connect(refreshBtn, SIGNAL(clicked()), this, SLOT(onRefreshClicked()));
    connect(webBtn, SIGNAL(clicked()), this, SLOT(onViewOnWeb()));
    connect(publishBtn, SIGNAL(clicked()), this, SLOT(onPublishClicked()));
    connect(chooseImageBtn, SIGNAL(clicked()), this, SLOT(onChooseImage()));
    connect(searchEdit, &QLineEdit::textChanged, [this](const QString&) {
        rebuildFeed(lastItems);
    });
}

void MemeStreamPage::setWalletModel(WalletModel* model)
{
    walletModel = model;
    updateAuthorLabel();
}

void MemeStreamPage::refresh()
{
    updateAuthorLabel();
    onRefreshClicked();
}

void MemeStreamPage::updateAuthorLabel()
{
    QString addr = currentAuthorAddress();
    if (addr.isEmpty())
        authorLabel->setText(tr("Author (wallet address — used for Tip): (no receiving address yet)"));
    else
        authorLabel->setText(tr("Author (wallet address — used for Tip):\n%1").arg(addr));
}

QString MemeStreamPage::currentAuthorAddress() const
{
    if (!walletModel)
        return QString();
    AddressTableModel* atm = walletModel->getAddressTableModel();
    if (!atm)
        return QString();
    // Prefer first receiving address in the address book.
    for (int row = 0; row < atm->rowCount(QModelIndex()); ++row) {
        QModelIndex idx = atm->index(row, AddressTableModel::Address, QModelIndex());
        QString type = atm->data(idx, AddressTableModel::TypeRole).toString();
        if (type == AddressTableModel::Receive) {
            QString addr = atm->data(idx, Qt::DisplayRole).toString();
            if (!addr.isEmpty())
                return addr;
        }
    }
    return QString();
}

void MemeStreamPage::onRefreshClicked()
{
    statusLabel->setText(tr("Loading feed…"));
    client->fetchFeed(30);
}

void MemeStreamPage::onViewOnWeb()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://memestream.gopastearth.com")));
}

void MemeStreamPage::onChooseImage()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Choose image"), QString(),
                                                tr("Images (*.png *.jpg *.jpeg *.gif *.webp)"));
    if (path.isEmpty())
        return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        statusLabel->setText(tr("Could not open image."));
        return;
    }
    QByteArray data = f.readAll();
    if (data.size() > MAX_IMAGE_BYTES) {
        statusLabel->setText(tr("Image too large (%1 bytes). Max is 69 KiB (%2 bytes).")
                                 .arg(data.size())
                                 .arg(MAX_IMAGE_BYTES));
        pendingImage.clear();
        pendingImageName.clear();
        imageLabel->setText(tr("No image (optional, ≤ 69 KiB)"));
        return;
    }
    pendingImage = data;
    pendingImageName = QFileInfo(path).fileName();
    imageLabel->setText(tr("%1 (%2 bytes)").arg(pendingImageName).arg(data.size()));
}

void MemeStreamPage::onPublishClicked()
{
    QString wallet = currentAuthorAddress();
    if (wallet.isEmpty()) {
        statusLabel->setText(tr("Need a receiving address in the wallet before publishing."));
        return;
    }
    if (!client->hasPublishKey()) {
        statusLabel->setText(tr("Set publish key: dogecoin-qt -memestreamkey=<key>"));
        Q_EMIT message(tr("Meme Stream"),
                       tr("Missing -memestreamkey. Use the same value as GPE MEMESTREAM_PUBLISH_KEY."),
                       CClientUIInterface::MSG_ERROR);
        return;
    }
    publishBtn->setEnabled(false);
    statusLabel->setText(tr("Publishing…"));
    client->publish(titleEdit->text().trimmed(),
                    bodyEdit->toPlainText().trimmed(),
                    wallet,
                    pendingImage,
                    pendingImageName);
}

void MemeStreamPage::onFeedReceived(const QList<MemeStreamItem>& items)
{
    lastItems = items;
    statusLabel->setText(tr("%1 memes · live · tip uses author wallet address").arg(items.size()));
    rebuildFeed(items);
}

void MemeStreamPage::onFeedFailed(const QString& error)
{
    statusLabel->setText(tr("Feed error: %1").arg(error));
}

void MemeStreamPage::onPublishSucceeded(const MemeStreamItem& /*item*/)
{
    publishBtn->setEnabled(true);
    statusLabel->setText(tr("Published."));
    titleEdit->clear();
    bodyEdit->clear();
    pendingImage.clear();
    pendingImageName.clear();
    imageLabel->setText(tr("No image (optional, ≤ 69 KiB)"));
    onRefreshClicked();
}

void MemeStreamPage::onPublishFailed(const QString& error)
{
    publishBtn->setEnabled(true);
    statusLabel->setText(tr("Publish failed: %1").arg(error));
    Q_EMIT message(tr("Meme Stream"), error, CClientUIInterface::MSG_ERROR);
}

void MemeStreamPage::onLikeSucceeded(const QString& /*itemId*/, int /*likeCount*/)
{
    onRefreshClicked();
}

void MemeStreamPage::onLikeFailed(const QString& error)
{
    statusLabel->setText(tr("Like failed: %1").arg(error));
}

void MemeStreamPage::rebuildFeed(const QList<MemeStreamItem>& items)
{
    // Clear layout
    while (QLayoutItem* child = feedLayout->takeAt(0)) {
        if (child->widget())
            child->widget()->deleteLater();
        delete child;
    }

    const QString filter = searchEdit->text().trimmed().toLower();
    int shown = 0;
    for (const MemeStreamItem& item : items) {
        if (!filter.isEmpty()) {
            const QString hay = (item.title + QLatin1Char(' ') + item.body + QLatin1Char(' ') + item.author).toLower();
            if (!hay.contains(filter))
                continue;
        }
        feedLayout->addWidget(buildItemCard(item));
        ++shown;
    }
    if (shown == 0) {
        QLabel* empty = new QLabel(filter.isEmpty() ? tr("No memes yet.") : tr("No matches."));
        empty->setAlignment(Qt::AlignCenter);
        feedLayout->addWidget(empty);
    }
    feedLayout->addStretch();
}

QWidget* MemeStreamPage::buildItemCard(const MemeStreamItem& item)
{
    QFrame* card = new QFrame();
    card->setObjectName("memeCard");
    card->setFrameShape(QFrame::StyledPanel);
    QVBoxLayout* lay = new QVBoxLayout(card);

    QLabel* t = new QLabel(item.title.isEmpty() ? tr("(untitled)") : item.title);
    t->setStyleSheet("font-weight: bold; font-size: 14px;");
    t->setWordWrap(true);
    lay->addWidget(t);

    if (!item.body.isEmpty()) {
        QLabel* b = new QLabel(item.body);
        b->setWordWrap(true);
        lay->addWidget(b);
    }

    QString tip = item.tipAddress.isEmpty() ? item.author : item.tipAddress;
    QString tipShort = tip;
    if (tipShort.size() > 16)
        tipShort = tipShort.left(8) + QStringLiteral("…") + tipShort.right(6);
    QLabel* tipLbl = new QLabel(tr("Tip → %1").arg(tipShort));
    tipLbl->setToolTip(tip);
    tipLbl->setStyleSheet("color: gray;");
    lay->addWidget(tipLbl);

    QHBoxLayout* actions = new QHBoxLayout();
    QPushButton* likeBtn = new QPushButton(tr("Like · %1").arg(item.likeCount));
    QPushButton* tipBtn = new QPushButton(tr("Tip"));
    QPushButton* boostBtn = new QPushButton(tr("Boost"));
    boostBtn->setToolTip(tr("Boost is a larger on-chain tip (uses Send)."));
    actions->addWidget(likeBtn);
    actions->addWidget(tipBtn);
    actions->addWidget(boostBtn);
    actions->addStretch();
    lay->addLayout(actions);

    const QString itemId = item.id;
    const QString tipAddr = tip;
    connect(likeBtn, &QPushButton::clicked, [this, itemId]() {
        client->likeItem(itemId, currentAuthorAddress());
    });
    connect(tipBtn, &QPushButton::clicked, [this, tipAddr]() {
        if (!tipAddr.isEmpty())
            Q_EMIT tipRequested(tipAddr);
    });
    connect(boostBtn, &QPushButton::clicked, [this, tipAddr]() {
        if (!tipAddr.isEmpty())
            Q_EMIT tipRequested(tipAddr);
    });

    return card;
}
