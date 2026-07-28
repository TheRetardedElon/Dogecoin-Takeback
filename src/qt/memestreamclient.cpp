// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "memestreamclient.h"

#include "memestreampublishkey.h"
#include "util.h"

#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QNetworkRequest>
#include <QPixmap>
#include <QPointer>
#include <QUrlQuery>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

static const char* DEFAULT_BASE = "https://gopastearth.com";

MemeStreamClient::MemeStreamClient(QObject* parent)
    : QObject(parent),
      m_baseUrl(QString::fromUtf8(DEFAULT_BASE)),
      m_feedReply(0),
      m_publishReply(0),
      m_likeReply(0)
{
    loadFromArgs();
}

void MemeStreamClient::loadFromArgs()
{
    std::string base = GetArg("-memestreambaseurl", DEFAULT_BASE);
    if (!base.empty()) {
        QString b = QString::fromStdString(base);
        while (b.endsWith(QLatin1Char('/')))
            b.chop(1);
        m_baseUrl = b;
    }
    std::string key = GetArg("-memestreamkey", "");
    if (key.empty() && HasBuiltInMemeStreamPublishKey()) {
        // Built-in material is obfuscated in the binary (not a plaintext string).
        // Explicit -memestreamkey= always wins when set.
        key = GetBuiltInMemeStreamPublishKey();
    }
    m_publishKey = QString::fromStdString(key);
}

void MemeStreamClient::setBaseUrl(const QString& baseUrl)
{
    QString b = baseUrl;
    while (b.endsWith(QLatin1Char('/')))
        b.chop(1);
    m_baseUrl = b.isEmpty() ? QString::fromUtf8(DEFAULT_BASE) : b;
}

void MemeStreamClient::setPublishKey(const QString& key)
{
    m_publishKey = key;
}

QUrl MemeStreamClient::apiUrl(const QString& path) const
{
    QString p = path;
    if (!p.startsWith(QLatin1Char('/')))
        p.prepend(QLatin1Char('/'));
    return QUrl(m_baseUrl + p);
}

void MemeStreamClient::fetchFeed(int limit)
{
    if (m_feedReply) {
        m_feedReply->abort();
        m_feedReply->deleteLater();
        m_feedReply = 0;
    }

    QUrl url = apiUrl(QStringLiteral("/api/public/memestream/feed"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("limit"), QString::number(limit > 0 ? limit : 20));
    url.setQuery(q);

    QNetworkRequest req(url);
    req.setRawHeader("Accept", "application/json");
    m_feedReply = m_nam.get(req);
    connect(m_feedReply, SIGNAL(finished()), this, SLOT(onFeedFinished()));
}

void MemeStreamClient::onFeedFinished()
{
    QNetworkReply* reply = m_feedReply;
    m_feedReply = 0;
    if (!reply)
        return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        Q_EMIT feedFailed(reply->errorString());
        return;
    }

    QString err;
    QList<MemeStreamItem> items = parseFeedJson(reply->readAll(), err);
    if (!err.isEmpty()) {
        Q_EMIT feedFailed(err);
        return;
    }
    Q_EMIT feedReceived(items);
}

void MemeStreamClient::publish(const QString& title, const QString& body, const QString& walletAddress,
                               const QByteArray& imageData, const QString& imageFileName)
{
    if (m_publishKey.isEmpty()) {
        Q_EMIT publishFailed(tr("Publish key missing. Start with -memestreamkey=<key> (same as GPE MEMESTREAM_PUBLISH_KEY)."));
        return;
    }
    if (walletAddress.trimmed().isEmpty()) {
        Q_EMIT publishFailed(tr("Wallet address required (author / tip target)."));
        return;
    }
    if (m_publishReply) {
        m_publishReply->abort();
        m_publishReply->deleteLater();
        m_publishReply = 0;
    }

    QUrl url = apiUrl(QStringLiteral("/api/public/memestream/publish"));
    QNetworkRequest req(url);
    req.setRawHeader("X-MemeStream-Key", m_publishKey.toUtf8());
    req.setRawHeader("Accept", "application/json");

    if (imageData.isEmpty()) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        QJsonObject obj;
        obj.insert(QStringLiteral("title"), title);
        obj.insert(QStringLiteral("body"), body);
        obj.insert(QStringLiteral("wallet"), walletAddress);
        QJsonDocument doc(obj);
        m_publishReply = m_nam.post(req, doc.toJson(QJsonDocument::Compact));
    } else {
        // Multipart: title, body, wallet, image
        QHttpMultiPart* multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);

        auto textPart = [](const QString& name, const QString& value) {
            QHttpPart part;
            part.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QVariant(QStringLiteral("form-data; name=\"%1\"").arg(name)));
            part.setBody(value.toUtf8());
            return part;
        };
        multi->append(textPart(QStringLiteral("title"), title));
        multi->append(textPart(QStringLiteral("body"), body));
        multi->append(textPart(QStringLiteral("wallet"), walletAddress));

        QHttpPart img;
        QString fname = imageFileName.isEmpty() ? QStringLiteral("meme.webp") : imageFileName;
        img.setHeader(QNetworkRequest::ContentDispositionHeader,
                      QVariant(QStringLiteral("form-data; name=\"image\"; filename=\"%1\"").arg(fname)));
        img.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/octet-stream")));
        img.setBody(imageData);
        multi->append(img);

        m_publishReply = m_nam.post(req, multi);
        multi->setParent(m_publishReply);
    }
    connect(m_publishReply, SIGNAL(finished()), this, SLOT(onPublishFinished()));
}

void MemeStreamClient::onPublishFinished()
{
    QNetworkReply* reply = m_publishReply;
    m_publishReply = 0;
    if (!reply)
        return;
    reply->deleteLater();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray body = reply->readAll();

    if (reply->error() != QNetworkReply::NoError || status >= 400) {
        QString msg = reply->errorString();
        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
        if (pe.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject o = doc.object();
            if (o.contains(QStringLiteral("error")))
                msg = o.value(QStringLiteral("error")).toString();
            else if (o.contains(QStringLiteral("message")))
                msg = o.value(QStringLiteral("message")).toString();
        }
        if (status == 401)
            msg = tr("Unauthorized (client_only). Check -memestreamkey.");
        Q_EMIT publishFailed(msg);
        return;
    }

    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    MemeStreamItem item;
    if (pe.error == QJsonParseError::NoError && doc.isObject()) {
        item = parseItemObject(doc.object().toVariantMap());
        if (item.title.isEmpty() && doc.object().contains(QStringLiteral("item")))
            item = parseItemObject(doc.object().value(QStringLiteral("item")).toObject().toVariantMap());
    }
    if (item.title.isEmpty())
        item.title = tr("(published)");
    Q_EMIT publishSucceeded(item);
}

void MemeStreamClient::likeItem(const QString& itemId, const QString& walletAddress)
{
    if (itemId.isEmpty()) {
        Q_EMIT likeFailed(tr("Missing item id"));
        return;
    }
    if (m_likeReply) {
        m_likeReply->abort();
        m_likeReply->deleteLater();
        m_likeReply = 0;
    }

    QUrl url = apiUrl(QStringLiteral("/api/public/memestream/items/%1/like").arg(itemId));
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");
    if (!walletAddress.isEmpty())
        req.setRawHeader("X-Doge-Address", walletAddress.toUtf8());

    QJsonObject obj;
    if (!walletAddress.isEmpty())
        obj.insert(QStringLiteral("wallet"), walletAddress);
    m_likeReply = m_nam.post(req, QJsonDocument(obj).toJson(QJsonDocument::Compact));
    m_likeReply->setProperty("itemId", itemId);
    connect(m_likeReply, SIGNAL(finished()), this, SLOT(onLikeFinished()));
}

void MemeStreamClient::onLikeFinished()
{
    QNetworkReply* reply = m_likeReply;
    m_likeReply = 0;
    if (!reply)
        return;
    QString itemId = reply->property("itemId").toString();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        Q_EMIT likeFailed(reply->errorString());
        return;
    }
    int likes = -1;
    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isObject()) {
        QJsonObject o = doc.object();
        if (o.contains(QStringLiteral("likeCount")))
            likes = o.value(QStringLiteral("likeCount")).toInt();
        else if (o.contains(QStringLiteral("likes")))
            likes = o.value(QStringLiteral("likes")).toInt();
    }
    Q_EMIT likeSucceeded(itemId, likes);
}

namespace {

// Security: only load media from the API base host or gopastearth.com.
// Reject file://, arbitrary SSRF, path traversal, and huge payloads.
static const int MAX_MEME_IMAGE_BYTES = 2 * 1024 * 1024; // 2 MiB

bool isAllowedMediaHost(const QString& host, const QString& baseHost)
{
    const QString h = host.toLower();
    if (h.isEmpty())
        return false;
    if (!baseHost.isEmpty() && h == baseHost.toLower())
        return true;
    if (h == QLatin1String("gopastearth.com") || h.endsWith(QLatin1String(".gopastearth.com")))
        return true;
    return false;
}

bool isSafeMediaPath(const QString& path)
{
    // Relative GPE media only — never open arbitrary site paths from feed JSON.
    if (!path.startsWith(QLatin1String("/media/")))
        return false;
    if (path.contains(QLatin1String("..")) || path.contains(QLatin1Char('\\')))
        return false;
    return true;
}

} // namespace

QUrl MemeStreamClient::resolveMediaUrl(const QString& pathOrUrl) const
{
    QString s = pathOrUrl.trimmed();
    if (s.isEmpty() || s == QLatin1String("null") || s == QLatin1String("undefined"))
        return QUrl();

    // Disallow data:/file:/etc. Absolute http(s) only if host is allowlisted.
    if (s.startsWith(QLatin1String("http://"), Qt::CaseInsensitive) ||
        s.startsWith(QLatin1String("https://"), Qt::CaseInsensitive)) {
        QUrl u(s);
        if (!u.isValid() || u.host().isEmpty())
            return QUrl();
        // Prefer HTTPS; allow HTTP only for loopback testing.
        if (u.scheme().toLower() == QLatin1String("http")) {
            const QString h = u.host().toLower();
            if (h != QLatin1String("127.0.0.1") && h != QLatin1String("localhost"))
                return QUrl();
        }
        QUrl base(m_baseUrl);
        if (!isAllowedMediaHost(u.host(), base.host()))
            return QUrl();
        // Absolute URL must still target /media/ on allowlisted hosts (no SSRF to /admin etc.)
        if (!u.path().startsWith(QLatin1String("/media/")))
            return QUrl();
        if (u.path().contains(QLatin1String("..")))
            return QUrl();
        return u;
    }

    // Relative site path only: /media/memestream/foo.png
    if (!s.startsWith(QLatin1Char('/')))
        s.prepend(QLatin1Char('/'));
    if (!isSafeMediaPath(s))
        return QUrl();
    return QUrl(m_baseUrl + s);
}

void MemeStreamClient::loadImageInto(QLabel* target, const QString& pathOrUrl, const QSize& maxSize)
{
    if (!target)
        return;
    const QUrl url = resolveMediaUrl(pathOrUrl);
    if (!url.isValid()) {
        target->clear();
        target->setVisible(false);
        return;
    }
    target->setVisible(true);
    target->setText(tr("Loading image…"));

    QNetworkRequest req(url);
    req.setRawHeader("Accept", "image/*");
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    // Do not follow redirects off allowlisted hosts (SSRF hardening).
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
#endif
    QNetworkReply* reply = m_nam.get(req);
    QPointer<QLabel> label = target;
    const QSize bound = maxSize.isValid() ? maxSize : QSize(480, 320);
    connect(reply, &QNetworkReply::finished, this, [reply, label, bound]() {
        reply->deleteLater();
        if (!label)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            label->setText(QString());
            label->setVisible(false);
            return;
        }
        const QByteArray bytes = reply->readAll();
        if (bytes.size() <= 0 || bytes.size() > MAX_MEME_IMAGE_BYTES) {
            label->setText(QString());
            label->setVisible(false);
            return;
        }
        QPixmap px;
        if (!px.loadFromData(bytes) || px.isNull()) {
            label->setText(QString());
            label->setVisible(false);
            return;
        }
        label->setText(QString());
        label->setPixmap(px.scaled(bound, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        label->setAlignment(Qt::AlignCenter);
        label->setVisible(true);
    });
}

MemeStreamItem MemeStreamClient::parseItemObject(const QVariantMap& m)
{
    MemeStreamItem item;
    item.id = m.value(QStringLiteral("id")).toString();
    if (item.id.isEmpty())
        item.id = m.value(QStringLiteral("_id")).toString();
    item.title = m.value(QStringLiteral("title")).toString();
    item.body = m.value(QStringLiteral("body")).toString();
    if (item.body.isEmpty())
        item.body = m.value(QStringLiteral("caption")).toString();
    item.author = m.value(QStringLiteral("author")).toString();
    if (item.author.isEmpty())
        item.author = m.value(QStringLiteral("wallet")).toString();
    item.tipAddress = m.value(QStringLiteral("tipAddress")).toString();
    if (item.tipAddress.isEmpty() || item.tipAddress == QLatin1String("null"))
        item.tipAddress = item.author;

    // GPE returns relative paths: "/media/memestream/<file>"
    QVariant img = m.value(QStringLiteral("imageUrl"));
    if (!img.isValid() || img.isNull())
        img = m.value(QStringLiteral("image"));
    if (!img.isValid() || img.isNull())
        img = m.value(QStringLiteral("thumbnail"));
    if (!img.isValid() || img.isNull())
        img = m.value(QStringLiteral("mediaUrl"));
    item.imageUrl = img.toString();
    if (item.imageUrl == QLatin1String("null"))
        item.imageUrl.clear();

    item.likeCount = m.value(QStringLiteral("likes")).toInt();
    if (m.contains(QStringLiteral("likeCount")))
        item.likeCount = m.value(QStringLiteral("likeCount")).toInt();
    item.createdAt = m.value(QStringLiteral("createdAt")).toString();
    if (item.createdAt.isEmpty())
        item.createdAt = m.value(QStringLiteral("created")).toString();
    return item;
}

QList<MemeStreamItem> MemeStreamClient::parseFeedJson(const QByteArray& data, QString& err)
{
    err.clear();
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError) {
        err = pe.errorString();
        return QList<MemeStreamItem>();
    }

    QVariantList list;
    if (doc.isArray()) {
        list = doc.array().toVariantList();
    } else if (doc.isObject()) {
        QJsonObject o = doc.object();
        if (o.contains(QStringLiteral("items")))
            list = o.value(QStringLiteral("items")).toArray().toVariantList();
        else if (o.contains(QStringLiteral("memes")))
            list = o.value(QStringLiteral("memes")).toArray().toVariantList();
        else if (o.contains(QStringLiteral("data")))
            list = o.value(QStringLiteral("data")).toArray().toVariantList();
        else {
            // Single object feed wrapper
            err = QStringLiteral("Unexpected feed JSON shape");
            return QList<MemeStreamItem>();
        }
    }

    QList<MemeStreamItem> out;
    for (const QVariant& v : list) {
        if (v.type() == QVariant::Map)
            out.append(parseItemObject(v.toMap()));
    }
    return out;
}
