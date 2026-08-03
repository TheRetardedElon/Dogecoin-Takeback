// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef DOGECOIN_QT_ARCADEGAMEWIDGET_H
#define DOGECOIN_QT_ARCADEGAMEWIDGET_H

#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QVector>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QTimer;
class QKeyEvent;
class QPaintEvent;
class QFocusEvent;
QT_END_NAMESPACE

/**
 * Retr-Doge Shibe Blaster — pure Qt retro arcade mini-game for Core Pro Arcade tab.
 * No wallet/network dependency; keyboard-only.
 */
class ArcadeGameWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ArcadeGameWidget(QWidget* parent = 0);

public Q_SLOTS:
    void startNewGame();
    void pauseGame();
    void resumeGame();

protected:
    void paintEvent(QPaintEvent* event);
    void keyPressEvent(QKeyEvent* event);
    void keyReleaseEvent(QKeyEvent* event);
    void focusInEvent(QFocusEvent* event);
    void focusOutEvent(QFocusEvent* event);
    void resizeEvent(QResizeEvent* event);

private Q_SLOTS:
    void tick();

private:
    enum GameState {
        StateTitle,
        StatePlaying,
        StatePaused,
        StateGameOver
    };

    struct Entity {
        QRectF rect;
        QPointF vel;
        int hp;
        int kind; // 0 player, 1 bullet, 2 hater, 3 fud, 4 wow particle
        int frame;
        bool alive;
    };

    void resetWorld();
    void spawnEnemy();
    void fire();
    void updatePlaying(double dt);
    void drawBackground(QPainter& p);
    void drawHud(QPainter& p);
    void drawTitle(QPainter& p);
    void drawGameOver(QPainter& p);
    void drawPlayer(QPainter& p, const Entity& e);
    void drawEnemy(QPainter& p, const Entity& e);
    void drawBullet(QPainter& p, const Entity& e);
    QRectF playfield() const;

    QTimer* timer;
    GameState state;
    QPixmap titleArt;

    Entity player;
    QVector<Entity> bullets;
    QVector<Entity> enemies;

    int score;
    int lives;
    int credits;
    int highScore;
    int wave;
    double spawnTimer;
    double invulnTimer;
    double starPhase;

    bool keyLeft;
    bool keyRight;
    bool keyFire;
    bool fireLatch;
};

#endif // DOGECOIN_QT_ARCADEGAMEWIDGET_H
