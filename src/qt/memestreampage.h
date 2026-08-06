// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_MEMESTREAMPAGE_H
#define DOGECOIN_QT_MEMESTREAMPAGE_H

#include "memestreamclient.h"

#include <QWidget>
#include <QList>

class WalletModel;
class PlatformStyle;

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QNetworkAccessManager;
class QListWidget;
QT_END_NAMESPACE

/**
 * Full Meme Stream page: publish form + feed.
 * Author / tip identity = Core wallet receive address.
 */
class MemeStreamPage : public QWidget
{
    Q_OBJECT

public:
    explicit MemeStreamPage(const PlatformStyle* platformStyle, QWidget* parent = 0);
    ~MemeStreamPage();

    void setWalletModel(WalletModel* walletModel);
    /** Refresh feed when page is shown. */
    void refresh();

Q_SIGNALS:
    void tipRequested(const QString& address);
    void message(const QString& title, const QString& msg, unsigned int style);

public Q_SLOTS:
    void onRefreshClicked();
    void onPublishClicked();
    void onChooseImage();
    void onViewOnWeb();
    void onFeedReceived(const QList<MemeStreamItem>& items);
    void onFeedFailed(const QString& error);
    void onPublishSucceeded(const MemeStreamItem& item);
    void onPublishFailed(const QString& error);
    void onLikeSucceeded(const QString& itemId, int likeCount);
    void onLikeFailed(const QString& error);

private:
    void setupUi();
    void ensureClient();
    void updateAuthorLabel();
    QString currentAuthorAddress() const;
    /** Existing receive address, or allocate one labeled for Meme Stream. */
    QString ensureAuthorAddress();
    void rebuildFeed(const QList<MemeStreamItem>& items);
    QWidget* buildItemCard(const MemeStreamItem& item);

    const PlatformStyle* platformStyle;
    WalletModel* walletModel;
    MemeStreamClient* client;

    QLabel* authorLabel;
    QLineEdit* titleEdit;
    QPlainTextEdit* bodyEdit;
    QLabel* imageLabel;
    QPushButton* chooseImageBtn;
    QPushButton* publishBtn;
    QPushButton* refreshBtn;
    QPushButton* webBtn;
    QLabel* statusLabel;
    QLineEdit* searchEdit;
    QScrollArea* feedScroll;
    QWidget* feedContainer;
    QVBoxLayout* feedLayout;

    QByteArray pendingImage;
    QString pendingImageName;
    QList<MemeStreamItem> lastItems;
};

#endif // DOGECOIN_QT_MEMESTREAMPAGE_H
