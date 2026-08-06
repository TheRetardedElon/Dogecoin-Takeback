// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_PEERMAPWIDGET_H
#define DOGECOIN_QT_PEERMAPWIDGET_H

#include "net.h"
#include "peertablemodel.h"

#include <QHash>
#include <QPointF>
#include <QString>
#include <QWidget>

class ClientModel;
class PeerTableModel;

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QLabel;
class QFrame;
class QTimer;
QT_END_NAMESPACE

/**
 * World-map view of connected peers (main Network page only).
 * Plots peer IPs after async geo lookup; click a dot for a detail card.
 */
class PeerMapWidget : public QWidget
{
    Q_OBJECT

public:
    struct PeerPin {
        NodeId nodeId;
        QString ip;
        QString addrName;
        QString subVer;
        QString direction; // Inbound / Outbound
        QString services;
        QString ping;
        int startingHeight;
        qint64 bytesSent;
        qint64 bytesRecv;
        double lat;
        double lon;
        bool hasGeo;
        QString city;
        QString country;
        QString isp;
        bool lookingUp;
    };

    explicit PeerMapWidget(QWidget* parent = 0);

    void setClientModel(ClientModel* model);

public Q_SLOTS:
    void refreshFromPeers();

protected:
    void paintEvent(QPaintEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
    void leaveEvent(QEvent* event);
    void resizeEvent(QResizeEvent* event);

private Q_SLOTS:
    void onPeerModelReset();
    void onGeoFinished();
    void hidePopup();

private:
    void ensureSsl();
    void scheduleLookups();
    void lookupIp(const QString& ip);
    void showPopupFor(const PeerPin& pin, const QPoint& globalPos);
    QRectF mapImageRect() const;
    QPointF latLonToWidget(double lat, double lon) const;
    int hitTestPin(const QPoint& pos) const;
    static QString extractIp(const QString& addrName);
    static bool isPublicIp(const QString& ip);
    void loadGeoCache();
    void saveGeoCacheEntry(const QString& ip, double lat, double lon,
                           const QString& city, const QString& country, const QString& isp);

    ClientModel* clientModel;
    QPixmap mapPixmap;
    QList<PeerPin> pins;
    QHash<QString, int> pinByIp; // ip -> index in pins
    QHash<QString, bool> pendingLookup;
    QNetworkAccessManager* nam;
    int hoverIndex;
    int selectedIndex;

    QFrame* popupFrame;
    QLabel* popupTitle;
    QLabel* popupBody;

    QTimer* refreshTimer;
};

#endif // DOGECOIN_QT_PEERMAPWIDGET_H
