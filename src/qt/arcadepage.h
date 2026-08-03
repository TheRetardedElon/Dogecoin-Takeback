// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_ARCADEPAGE_H
#define DOGECOIN_QT_ARCADEPAGE_H

#include <QWidget>

class PlatformStyle;
class ArcadeGameWidget;

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
QT_END_NAMESPACE

/**
 * Core Pro Arcade — retro mini-games shelf (client only, not settlement node).
 */
class ArcadePage : public QWidget
{
    Q_OBJECT

public:
    explicit ArcadePage(const PlatformStyle* platformStyle, QWidget* parent = 0);

public Q_SLOTS:
    void focusGame();
    void onPlayClicked();

private:
    void setupUi();

    const PlatformStyle* platformStyle;
    ArcadeGameWidget* game;
    QPushButton* playBtn;
};

#endif // DOGECOIN_QT_ARCADEPAGE_H
