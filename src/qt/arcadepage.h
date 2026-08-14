// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_ARCADEPAGE_H
#define DOGECOIN_QT_ARCADEPAGE_H

#include <QWidget>
#include <QVector>

class PlatformStyle;
class ArcadeGameWidget;

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QStackedWidget;
class QScrollArea;
class QButtonGroup;
class QFrame;
QT_END_NAMESPACE

/**
 * Core Pro Arcade — multi-game cabinet shelf.
 * Local Qt games (Shibe Blaster) + web titles (Flappy Doge) + GPE hub link.
 * Client-only fun — not settlement / consensus.
 */
class ArcadePage : public QWidget
{
    Q_OBJECT

public:
    explicit ArcadePage(const PlatformStyle* platformStyle, QWidget* parent = 0);

public Q_SLOTS:
    void focusGame();
    void onPlayClicked();
    void onOpenHubClicked();
    void onCategoryChanged(int id);
    void onGameSelected(int index);
    void onLaunchSelected();

private:
    enum GameKind {
        GameLocalBlaster = 0,
        GameWebFlappy,
        GameComingSoon
    };

    struct GameEntry {
        QString id;
        QString title;
        QString category; // Classic | Action | Puzzle | Racing | All
        QString blurb;
        QString badge;    // Featured / Local / Web / Soon
        GameKind kind;
        QString url;      // external launch URL when web
        bool featured;
    };

    void setupUi();
    void rebuildCatalog();
    void rebuildShelf();
    void showCabinetFor(int index);
    QFrame* makeGameCard(int index, const GameEntry& g);

    const PlatformStyle* platformStyle;

    QVector<GameEntry> catalog;
    QString activeCategory; // "All" or category name
    int selectedIndex;

    QLabel* titleLabel;
    QLabel* subLabel;
    QPushButton* hubBtn;
    QPushButton* playBtn;
    QButtonGroup* categoryGroup;
    QWidget* categoryBar;
    QScrollArea* shelfScroll;
    QWidget* shelfHost;
    QStackedWidget* stage;
    QWidget* shelfPage;
    QWidget* playPage;
    ArcadeGameWidget* blaster;
    QLabel* webTitle;
    QLabel* webBlurb;
    QPushButton* webLaunchBtn;
    QPushButton* backToShelfBtn;
};

#endif // DOGECOIN_QT_ARCADEPAGE_H
