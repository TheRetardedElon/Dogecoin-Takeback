// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_MEMESTREAMCLIENT_H
#define DOGECOIN_QT_MEMESTREAMCLIENT_H

#include <QObject>
#include <QString>
#include <QList>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QSize>

class QLabel;

/**
 * HTTP client for GoPastEarth MemeStream public API.
 * Does not use Dogecoin RPC. Publish requires -memestreamkey=.
 *
 * Canonical base: https://gopastearth.com (override: -memestreambaseurl=)
 * Feed:   GET  /api/public/memestream/feed
 * Publish: POST /api/public/memestream/publish  (header X-MemeStream-Key)
 * Like:   POST /api/public/memestream/items/:id/like
 *
 * Windows: do not use setDefaultCaCertificates with a rebuilt ROOT store
 * (heap corruption on mingw-Qt). Per-request systemCaCertificates() instead.
 */
struct MemeStreamItem {
    QString id;
    QString title;
    QString body;
    QString author;       //!< DOGE address (also tip target)
    QString tipAddress;
    QString imageUrl;
    int likeCount;
    QString createdAt;
};

class MemeStreamClient : public QObject
{
    Q_OBJECT

public:
    explicit MemeStreamClient(QObject* parent = 0);

    /** Base URL without trailing slash (default https://gopastearth.com). */
    void setBaseUrl(const QString& baseUrl);
    QString baseUrl() const { return m_baseUrl; }

    void setPublishKey(const QString& key);
    QString publishKey() const { return m_publishKey; }

    bool hasPublishKey() const { return !m_publishKey.isEmpty(); }

    void fetchFeed(int limit = 20);
    void publish(const QString& title, const QString& body, const QString& walletAddress,
                 const QByteArray& imageData = QByteArray(), const QString& imageFileName = QString());
    void likeItem(const QString& itemId, const QString& walletAddress);

    /**
     * Resolve relative media paths (e.g. /media/memestream/x.png) against baseUrl.
     * Absolute https URLs are returned unchanged.
     */
    QUrl resolveMediaUrl(const QString& pathOrUrl) const;

    /**
     * Async-load an image into a QLabel (scaled to maxSize, KeepAspectRatio).
     * Safe if the label is destroyed before the reply finishes.
     */
    void loadImageInto(QLabel* target, const QString& pathOrUrl, const QSize& maxSize);

    /** Load defaults from -memestreambaseurl / -memestreamkey. */
    void loadFromArgs();

Q_SIGNALS:
    void feedReceived(const QList<MemeStreamItem>& items);
    void feedFailed(const QString& error);
    void publishSucceeded(const MemeStreamItem& item);
    void publishFailed(const QString& error);
    void likeSucceeded(const QString& itemId, int likeCount);
    void likeFailed(const QString& error);

private Q_SLOTS:
    void onFeedFinished();
    void onPublishFinished();
    void onLikeFinished();

private:
    QUrl apiUrl(const QString& path) const;
    static QList<MemeStreamItem> parseFeedJson(const QByteArray& data, QString& err);
    static MemeStreamItem parseItemObject(const QVariantMap& m);

    QNetworkAccessManager m_nam;
    QString m_baseUrl;
    QString m_publishKey;
    QNetworkReply* m_feedReply;
    QNetworkReply* m_publishReply;
    QNetworkReply* m_likeReply;
};

#endif // DOGECOIN_QT_MEMESTREAMCLIENT_H
