// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "peermapwidget.h"

#include "clientmodel.h"
#include "guiutil.h"
#include "peertablemodel.h"
#include "util.h"
#include "utiltime.h"

#include <QEvent>
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QShowEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QUrl>

#include <cmath>

// Note: do not mutate the process-wide QSslSocket CA store from here.
// Injecting system ROOT certs via CertEnumCertificatesInStore +
// setDefaultCaCertificates has been associated with heap corruption
// (STATUS_HEAP_CORRUPTION / 0xc0000374) on some Windows + mingw-Qt builds.
// Geo lookups use plain HTTP (ip-api.com), so extra SSL setup is unnecessary.

PeerMapWidget::PeerMapWidget(QWidget* parent)
    : QWidget(parent),
      clientModel(0),
      nam(0),
      hoverIndex(-1),
      selectedIndex(-1),
      popupFrame(0),
      popupTitle(0),
      popupBody(0),
      refreshTimer(0)
{
    setObjectName(QStringLiteral("peerMapWidget"));
    setMouseTracking(true);
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    mapPixmap = QPixmap(QStringLiteral(":/images/worldmap-yellow"));
    if (mapPixmap.isNull()) {
        // Fallback placeholder so layout still works if asset missing
        mapPixmap = QPixmap(800, 400);
        mapPixmap.fill(QColor(20, 24, 32));
    }

    // Lazy: create NAM only when a lookup is requested (avoids Qt network
    // stack init during wallet page construction at startup).
    nam = 0;
    loadGeoCache();

    // Floating card (child of this widget, raised above map)
    popupFrame = new QFrame(this);
    popupFrame->setObjectName(QStringLiteral("peerMapPopup"));
    popupFrame->setFrameShape(QFrame::StyledPanel);
    popupFrame->setAutoFillBackground(true);
    popupFrame->setStyleSheet(
        QStringLiteral(
            "QFrame#peerMapPopup {"
            "  background-color: rgba(18, 22, 30, 230);"
            "  border: 1px solid #f5c518;"
            "  border-radius: 10px;"
            "}"
            "QLabel { color: #f0f0f0; background: transparent; }"
            "QLabel#peerMapPopupTitle { color: #f5c518; font-weight: bold; font-size: 13px; }"));
    QVBoxLayout* pl = new QVBoxLayout(popupFrame);
    pl->setContentsMargins(12, 10, 12, 10);
    pl->setSpacing(4);
    popupTitle = new QLabel(popupFrame);
    popupTitle->setObjectName(QStringLiteral("peerMapPopupTitle"));
    popupTitle->setWordWrap(true);
    popupBody = new QLabel(popupFrame);
    popupBody->setWordWrap(true);
    popupBody->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pl->addWidget(popupTitle);
    pl->addWidget(popupBody);
    popupFrame->setFixedWidth(280);
    popupFrame->hide();

    refreshTimer = new QTimer(this);
    refreshTimer->setInterval(4000);
    connect(refreshTimer, SIGNAL(timeout()), this, SLOT(refreshFromPeers()));
}

void PeerMapWidget::setClientModel(ClientModel* model)
{
    if (clientModel && clientModel->getPeerTableModel()) {
        disconnect(clientModel->getPeerTableModel(), 0, this, 0);
    }
    clientModel = model;
    if (clientModel && clientModel->getPeerTableModel()) {
        connect(clientModel->getPeerTableModel(), SIGNAL(layoutChanged()),
                this, SLOT(onPeerModelReset()));
        connect(clientModel->getPeerTableModel(), SIGNAL(modelReset()),
                this, SLOT(onPeerModelReset()));
        // Do not start peer-table auto-refresh or map timer until this widget is shown.
        // Network page is constructed at wallet open but not visible yet — starting
        // timers/QNAM here raced with startup and contributed to Windows heap faults.
        if (isVisible()) {
            clientModel->getPeerTableModel()->startAutoRefresh();
            if (refreshTimer)
                refreshTimer->start();
            refreshFromPeers();
        }
    } else {
        if (refreshTimer)
            refreshTimer->stop();
        refreshFromPeers();
    }
}

void PeerMapWidget::onPeerModelReset()
{
    refreshFromPeers();
}

QString PeerMapWidget::extractIp(const QString& addrName)
{
    // Formats: "1.2.3.4:22556", "[2001:db8::1]:22556", "hostname:port"
    QString s = addrName.trimmed();
    if (s.startsWith(QLatin1Char('['))) {
        const int end = s.indexOf(QLatin1Char(']'));
        if (end > 1)
            return s.mid(1, end - 1);
    }
    // strip :port for IPv4 (last colon)
    const int colon = s.lastIndexOf(QLatin1Char(':'));
    if (colon > 0 && s.count(QLatin1Char(':')) == 1)
        return s.left(colon);
    // IPv6 without brackets unlikely in addrName; return as-is
    if (colon > 0 && s.count(QLatin1Char(':')) > 1) {
        // might be host:port with hostname only
        if (!s.contains(QLatin1Char('.')))
            return s; // leave
        return s.left(colon);
    }
    return s;
}

bool PeerMapWidget::isPublicIp(const QString& ip)
{
    if (ip.isEmpty())
        return false;
    if (ip == QLatin1String("127.0.0.1") || ip == QLatin1String("::1"))
        return false;
    if (ip.startsWith(QLatin1String("10.")))
        return false;
    if (ip.startsWith(QLatin1String("192.168.")))
        return false;
    if (ip.startsWith(QLatin1String("169.254.")))
        return false;
    // 172.16.0.0 – 172.31.255.255
    if (ip.startsWith(QLatin1String("172."))) {
        const QStringList parts = ip.split(QLatin1Char('.'));
        if (parts.size() >= 2) {
            bool ok = false;
            const int second = parts.at(1).toInt(&ok);
            if (ok && second >= 16 && second <= 31)
                return false;
        }
    }
    if (ip.startsWith(QLatin1String("fc")) || ip.startsWith(QLatin1String("fd")) ||
        ip.startsWith(QLatin1String("fe80"), Qt::CaseInsensitive))
        return false;
    return true;
}

void PeerMapWidget::loadGeoCache()
{
    // Cache is loaded per-IP during lookup from QSettings groups
}

void PeerMapWidget::saveGeoCacheEntry(const QString& ip, double lat, double lon,
                                     const QString& city, const QString& country, const QString& isp)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("peerGeoCache"));
    settings.beginGroup(ip);
    settings.setValue(QStringLiteral("lat"), lat);
    settings.setValue(QStringLiteral("lon"), lon);
    settings.setValue(QStringLiteral("city"), city);
    settings.setValue(QStringLiteral("country"), country);
    settings.setValue(QStringLiteral("isp"), isp);
    settings.setValue(QStringLiteral("ts"), static_cast<qlonglong>(GetTime()));
    settings.endGroup();
    settings.endGroup();
}

void PeerMapWidget::refreshFromPeers()
{
    pins.clear();
    pinByIp.clear();
    if (!clientModel || !clientModel->getPeerTableModel()) {
        update();
        return;
    }

    PeerTableModel* model = clientModel->getPeerTableModel();
    const int n = model->rowCount(QModelIndex());
    for (int row = 0; row < n; ++row) {
        const CNodeCombinedStats* stats = model->getNodeStats(row);
        if (!stats)
            continue;

        PeerPin pin;
        pin.nodeId = stats->nodeStats.nodeid;
        pin.addrName = QString::fromStdString(stats->nodeStats.addrName);
        pin.ip = extractIp(pin.addrName);
        pin.subVer = QString::fromStdString(stats->nodeStats.cleanSubVer);
        pin.direction = stats->nodeStats.fInbound ? tr("Inbound") : tr("Outbound");
        pin.services = GUIUtil::formatServicesStr(static_cast<quint64>(stats->nodeStats.nServices));
        pin.ping = GUIUtil::formatPingTime(stats->nodeStats.dPingTime);
        pin.startingHeight = stats->nodeStats.nStartingHeight;
        pin.bytesSent = static_cast<qint64>(stats->nodeStats.nSendBytes);
        pin.bytesRecv = static_cast<qint64>(stats->nodeStats.nRecvBytes);
        pin.hasGeo = false;
        pin.lookingUp = false;
        pin.lat = 0;
        pin.lon = 0;

        // Apply cache if present
        if (isPublicIp(pin.ip)) {
            QSettings settings;
            settings.beginGroup(QStringLiteral("peerGeoCache"));
            settings.beginGroup(pin.ip);
            if (settings.contains(QStringLiteral("lat"))) {
                pin.lat = settings.value(QStringLiteral("lat")).toDouble();
                pin.lon = settings.value(QStringLiteral("lon")).toDouble();
                pin.city = settings.value(QStringLiteral("city")).toString();
                pin.country = settings.value(QStringLiteral("country")).toString();
                pin.isp = settings.value(QStringLiteral("isp")).toString();
                pin.hasGeo = true;
            }
            settings.endGroup();
            settings.endGroup();
        }

        pinByIp.insert(pin.ip, pins.size());
        pins.append(pin);
    }

    scheduleLookups();
    update();

    // Keep selected popup in sync
    if (selectedIndex >= 0 && selectedIndex < pins.size()) {
        QPoint g = mapToGlobal(latLonToWidget(pins[selectedIndex].lat, pins[selectedIndex].lon).toPoint());
        showPopupFor(pins[selectedIndex], g);
    } else if (selectedIndex >= pins.size()) {
        selectedIndex = -1;
        hidePopup();
    }
}

void PeerMapWidget::scheduleLookups()
{
    int queued = 0;
    for (int i = 0; i < pins.size(); ++i) {
        PeerPin& pin = pins[i];
        if (pin.hasGeo || pin.lookingUp)
            continue;
        if (!isPublicIp(pin.ip))
            continue;
        if (pendingLookup.value(pin.ip, false))
            continue;
        // Rate-limit free geolocation: a few at a time
        if (queued >= 3)
            break;
        pin.lookingUp = true;
        lookupIp(pin.ip);
        ++queued;
    }
}

void PeerMapWidget::lookupIp(const QString& ip)
{
    if (ip.isEmpty())
        return;
    if (!nam)
        nam = new QNetworkAccessManager(this);
    pendingLookup.insert(ip, true);

    // ip-api.com free HTTP endpoint (no key). HTTPS also available.
    // fields limit payload; status checks success.
    const QUrl url(QStringLiteral("http://ip-api.com/json/%1?fields=status,message,country,city,lat,lon,isp,org,as,query")
                       .arg(ip));
    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "DogecoinCorePro-PeerMap/1.0");
    QNetworkReply* reply = nam->get(req);
    if (!reply) {
        pendingLookup.remove(ip);
        return;
    }
    reply->setProperty("lookupIp", ip);
    connect(reply, SIGNAL(finished()), this, SLOT(onGeoFinished()));
}

void PeerMapWidget::onGeoFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply)
        return;
    reply->deleteLater();

    const QString ip = reply->property("lookupIp").toString();
    pendingLookup.remove(ip);

    if (reply->error() != QNetworkReply::NoError) {
        LogPrintf("PeerMap: geo lookup failed for %s: %s\n",
                  ip.toStdString(), reply->errorString().toStdString());
        // clear lookingUp flags
        if (pinByIp.contains(ip)) {
            const int idx = pinByIp.value(ip);
            if (idx >= 0 && idx < pins.size())
                pins[idx].lookingUp = false;
        }
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("status")).toString() != QLatin1String("success")) {
        if (pinByIp.contains(ip)) {
            const int idx = pinByIp.value(ip);
            if (idx >= 0 && idx < pins.size())
                pins[idx].lookingUp = false;
        }
        return;
    }

    const double lat = o.value(QStringLiteral("lat")).toDouble();
    const double lon = o.value(QStringLiteral("lon")).toDouble();
    const QString city = o.value(QStringLiteral("city")).toString();
    const QString country = o.value(QStringLiteral("country")).toString();
    QString isp = o.value(QStringLiteral("isp")).toString();
    if (isp.isEmpty())
        isp = o.value(QStringLiteral("org")).toString();

    saveGeoCacheEntry(ip, lat, lon, city, country, isp);

    // Update any pins with this IP (refresh may have rebuilt list)
    for (int i = 0; i < pins.size(); ++i) {
        if (pins[i].ip == ip) {
            pins[i].lat = lat;
            pins[i].lon = lon;
            pins[i].city = city;
            pins[i].country = country;
            pins[i].isp = isp;
            pins[i].hasGeo = true;
            pins[i].lookingUp = false;
        }
    }
    update();
    // Continue remaining lookups
    scheduleLookups();
}

QRectF PeerMapWidget::mapImageRect() const
{
    if (mapPixmap.isNull() || width() < 10 || height() < 10)
        return QRectF();

    const QSize avail(width(), height());
    QSize scaled = mapPixmap.size();
    scaled.scale(avail, Qt::KeepAspectRatio);
    const qreal x = (avail.width() - scaled.width()) / 2.0;
    const qreal y = (avail.height() - scaled.height()) / 2.0;
    return QRectF(x, y, scaled.width(), scaled.height());
}

QPointF PeerMapWidget::latLonToWidget(double lat, double lon) const
{
    // Equirectangular projection (standard for flat world maps)
    const QRectF r = mapImageRect();
    if (!r.isValid())
        return QPointF();
    const qreal x = r.left() + ((lon + 180.0) / 360.0) * r.width();
    const qreal y = r.top() + ((90.0 - lat) / 180.0) * r.height();
    return QPointF(x, y);
}

int PeerMapWidget::hitTestPin(const QPoint& pos) const
{
    const qreal hitR = 10.0;
    int best = -1;
    qreal bestD = hitR * hitR;
    for (int i = 0; i < pins.size(); ++i) {
        if (!pins[i].hasGeo)
            continue;
        const QPointF p = latLonToWidget(pins[i].lat, pins[i].lon);
        const qreal dx = p.x() - pos.x();
        const qreal dy = p.y() - pos.y();
        const qreal d = dx * dx + dy * dy;
        if (d <= bestD) {
            bestD = d;
            best = i;
        }
    }
    return best;
}

void PeerMapWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Background
    p.fillRect(rect(), QColor(12, 14, 18));

    const QRectF r = mapImageRect();
    if (r.isValid() && !mapPixmap.isNull()) {
        p.drawPixmap(r.toRect(), mapPixmap);
    }

    // Legend / status
    int mapped = 0, pending = 0, localN = 0;
    for (const PeerPin& pin : pins) {
        if (pin.hasGeo)
            ++mapped;
        else if (!isPublicIp(pin.ip))
            ++localN;
        else
            ++pending;
    }
    p.setPen(QColor(200, 200, 200));
    QFont f = font();
    f.setPointSize(qMax(9, f.pointSize() - 1));
    p.setFont(f);
    const QString status = tr("Peers on map: %1  ·  Locating: %2  ·  Local/private: %3    ● magenta = outbound    ● cyan = inbound")
                               .arg(mapped)
                               .arg(pending)
                               .arg(localN);
    p.drawText(QRect(8, 6, width() - 16, 20), Qt::AlignLeft | Qt::AlignVCenter, status);

    // Draw pins — high contrast on yellow map (not gold-on-gold)
    // Outbound: hot magenta/pink · Inbound: electric cyan · dark ring so they pop
    for (int i = 0; i < pins.size(); ++i) {
        const PeerPin& pin = pins[i];
        if (!pin.hasGeo)
            continue;
        const QPointF pt = latLonToWidget(pin.lat, pin.lon);
        const bool hot = (i == hoverIndex || i == selectedIndex);
        const qreal radius = hot ? 8.0 : 6.0;
        const bool inbound = (pin.direction == tr("Inbound"));
        const QColor fill = inbound ? QColor(0, 230, 255) : QColor(255, 40, 180); // cyan / magenta
        const QColor glow = inbound ? QColor(0, 230, 255, hot ? 110 : 70)
                                    : QColor(255, 40, 180, hot ? 110 : 70);

        // Soft glow
        p.setBrush(glow);
        p.setPen(Qt::NoPen);
        p.drawEllipse(pt, radius + 5, radius + 5);

        // Solid core + dark outline (readable on yellow continents)
        p.setBrush(fill);
        p.setPen(QPen(QColor(10, 12, 16), hot ? 2.2 : 1.8));
        p.drawEllipse(pt, radius, radius);

        // Tiny white highlight for depth
        p.setBrush(QColor(255, 255, 255, 180));
        p.setPen(Qt::NoPen);
        p.drawEllipse(pt + QPointF(-radius * 0.25, -radius * 0.25), radius * 0.28, radius * 0.28);
    }

    // Unmapped strip (tiny chips bottom-left)
    if (localN + pending > 0 && mapped == 0) {
        p.setPen(QColor(160, 160, 160));
        p.drawText(QRect(8, height() - 28, width() - 16, 20), Qt::AlignLeft,
                   tr("Waiting for geo data for connected peers…"));
    }
}

void PeerMapWidget::mouseMoveEvent(QMouseEvent* event)
{
    const int hit = hitTestPin(event->pos());
    if (hit != hoverIndex) {
        hoverIndex = hit;
        if (hit >= 0) {
            const PeerPin& pin = pins[hit];
            setCursor(Qt::PointingHandCursor);
            QString tip = pin.city.isEmpty() ? pin.country : (pin.city + QStringLiteral(", ") + pin.country);
            if (tip.isEmpty())
                tip = pin.ip;
            tip += QStringLiteral("\n") + pin.subVer;
            QToolTip::showText(event->globalPos(), tip, this);
        } else {
            setCursor(Qt::ArrowCursor);
            QToolTip::hideText();
        }
        update();
    }
    QWidget::mouseMoveEvent(event);
}

void PeerMapWidget::leaveEvent(QEvent* event)
{
    hoverIndex = -1;
    setCursor(Qt::ArrowCursor);
    update();
    QWidget::leaveEvent(event);
}

void PeerMapWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int hit = hitTestPin(event->pos());
        if (hit >= 0) {
            selectedIndex = hit;
            showPopupFor(pins[hit], event->globalPos());
            update();
            return;
        }
        // click empty map → dismiss card
        selectedIndex = -1;
        hidePopup();
        update();
    }
    QWidget::mousePressEvent(event);
}

void PeerMapWidget::showPopupFor(const PeerPin& pin, const QPoint& globalPos)
{
    if (!popupFrame)
        return;

    QString title = pin.city.isEmpty() ? pin.country : (pin.city + QStringLiteral(", ") + pin.country);
    if (title.isEmpty())
        title = tr("Peer node");
    popupTitle->setText(title);

    QString body;
    body += tr("<b>Address</b>: %1<br/>").arg(pin.addrName.toHtmlEscaped());
    body += tr("<b>Node id</b>: %1<br/>").arg(pin.nodeId);
    body += tr("<b>Agent</b>: %1<br/>").arg(pin.subVer.toHtmlEscaped());
    body += tr("<b>Direction</b>: %1<br/>").arg(pin.direction);
    body += tr("<b>Services</b>: %1<br/>").arg(pin.services.toHtmlEscaped());
    body += tr("<b>Ping</b>: %1<br/>").arg(pin.ping);
    body += tr("<b>Starting height</b>: %1<br/>").arg(pin.startingHeight);
    body += tr("<b>Traffic</b>: ↓ %1  ↑ %2<br/>")
                .arg(GUIUtil::formatBytes(static_cast<uint64_t>(pin.bytesRecv)))
                .arg(GUIUtil::formatBytes(static_cast<uint64_t>(pin.bytesSent)));
    if (!pin.isp.isEmpty())
        body += tr("<b>Network / ISP</b>: %1<br/>").arg(pin.isp.toHtmlEscaped());
    if (pin.hasGeo)
        body += tr("<b>Coords</b>: %1, %2")
                    .arg(pin.lat, 0, 'f', 2)
                    .arg(pin.lon, 0, 'f', 2);

    popupBody->setText(body);
    popupFrame->adjustSize();

    // Place near click, clamp inside widget
    QPoint local = mapFromGlobal(globalPos);
    int x = local.x() + 12;
    int y = local.y() + 12;
    if (x + popupFrame->width() > width() - 8)
        x = width() - popupFrame->width() - 8;
    if (y + popupFrame->height() > height() - 8)
        y = height() - popupFrame->height() - 8;
    if (x < 8)
        x = 8;
    if (y < 8)
        y = 8;
    popupFrame->move(x, y);
    popupFrame->show();
    popupFrame->raise();
}

void PeerMapWidget::hidePopup()
{
    if (popupFrame)
        popupFrame->hide();
}

void PeerMapWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void PeerMapWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (clientModel && clientModel->getPeerTableModel()) {
        clientModel->getPeerTableModel()->startAutoRefresh();
        if (refreshTimer && !refreshTimer->isActive())
            refreshTimer->start();
        refreshFromPeers();
    }
}
