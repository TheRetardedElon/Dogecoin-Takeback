// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "arcadegamewidget.h"

#include <QFocusEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QSettings>
#include <QTimer>
#include <QtGlobal>
#include <cmath>
#include <ctime>

static const int TICK_MS = 16;
static const double PLAYER_SPEED = 320.0;
static const double BULLET_SPEED = 520.0;

ArcadeGameWidget::ArcadeGameWidget(QWidget* parent)
    : QWidget(parent),
      timer(0),
      state(StateTitle),
      score(0),
      lives(3),
      credits(50),
      highScore(0),
      wave(1),
      spawnTimer(0),
      invulnTimer(0),
      starPhase(0),
      keyLeft(false),
      keyRight(false),
      keyFire(false),
      fireLatch(false)
{
    setObjectName(QStringLiteral("arcadeGameWidget"));
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(480, 360);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    titleArt = QPixmap(QStringLiteral(":/images/retrdoge-title"));
    QSettings s;
    highScore = s.value(QStringLiteral("arcade/shibeBlaster/highScore"), 0).toInt();
    qsrand(static_cast<uint>(time(0)) ^ reinterpret_cast<quintptr>(this));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
    timer->start(TICK_MS);

    resetWorld();
}

void ArcadeGameWidget::startNewGame()
{
    if (credits <= 0)
        credits = 50; // free play at the doge arcade
    score = 0;
    lives = 3;
    wave = 1;
    spawnTimer = 0.6;
    invulnTimer = 1.5;
    bullets.clear();
    enemies.clear();
    resetWorld();
    state = StatePlaying;
    setFocus();
    update();
}

void ArcadeGameWidget::pauseGame()
{
    if (state == StatePlaying)
        state = StatePaused;
}

void ArcadeGameWidget::resumeGame()
{
    if (state == StatePaused)
        state = StatePlaying;
}

void ArcadeGameWidget::resetWorld()
{
    const QRectF pf = playfield();
    player.rect = QRectF(pf.center().x() - 22, pf.bottom() - 70, 44, 36);
    player.vel = QPointF(0, 0);
    player.hp = 1;
    player.kind = 0;
    player.frame = 0;
    player.alive = true;
}

QRectF ArcadeGameWidget::playfield() const
{
    // Leave room for neon bezel padding
    return QRectF(12, 12, qMax(100.0, width() - 24.0), qMax(100.0, height() - 24.0));
}

void ArcadeGameWidget::spawnEnemy()
{
    const QRectF pf = playfield();
    Entity e;
    const bool fud = (qrand() % 100) < (15 + wave);
    e.kind = fud ? 3 : 2;
    const double w = fud ? 36 : 28;
    const double h = fud ? 28 : 26;
    const double x = pf.left() + 20 + (qrand() % qMax(1, (int)(pf.width() - 40 - w)));
    e.rect = QRectF(x, pf.top() - h, w, h);
    const double speed = 40.0 + wave * 8.0 + (qrand() % 40);
    e.vel = QPointF((qrand() % 2 ? 1 : -1) * (20.0 + qrand() % 40), speed);
    e.hp = fud ? 2 : 1;
    e.frame = qrand() % 8;
    e.alive = true;
    enemies.append(e);
}

void ArcadeGameWidget::fire()
{
    if (!player.alive)
        return;
    Entity b;
    b.kind = 1;
    b.rect = QRectF(player.rect.center().x() - 3, player.rect.top() - 14, 6, 14);
    b.vel = QPointF(0, -BULLET_SPEED);
    b.hp = 1;
    b.frame = 0;
    b.alive = true;
    bullets.append(b);
    // side WOW sparkles
    for (int i = 0; i < 2; ++i) {
        Entity p;
        p.kind = 4;
        p.rect = QRectF(player.rect.center().x() + (i ? 10 : -16), player.rect.top(), 12, 10);
        p.vel = QPointF((i ? 40 : -40), -180);
        p.hp = 1;
        p.frame = 0;
        p.alive = true;
        bullets.append(p);
    }
}

void ArcadeGameWidget::tick()
{
    const double dt = TICK_MS / 1000.0;
    starPhase += dt;
    if (state == StatePlaying)
        updatePlaying(dt);
    update();
}

void ArcadeGameWidget::updatePlaying(double dt)
{
    const QRectF pf = playfield();

    // movement
    double vx = 0;
    if (keyLeft)
        vx -= PLAYER_SPEED;
    if (keyRight)
        vx += PLAYER_SPEED;
    player.rect.translate(vx * dt, 0);
    if (player.rect.left() < pf.left())
        player.rect.moveLeft(pf.left());
    if (player.rect.right() > pf.right())
        player.rect.moveRight(pf.right());

    if (keyFire && !fireLatch) {
        fire();
        fireLatch = true;
    }
    if (!keyFire)
        fireLatch = false;

    if (invulnTimer > 0)
        invulnTimer -= dt;

    // bullets
    for (int i = 0; i < bullets.size(); ++i) {
        Entity& b = bullets[i];
        if (!b.alive)
            continue;
        b.rect.translate(b.vel.x() * dt, b.vel.y() * dt);
        b.frame++;
        if (b.kind == 4 && b.frame > 18)
            b.alive = false;
        if (!pf.adjusted(-40, -40, 40, 40).intersects(b.rect))
            b.alive = false;
    }

    // enemies
    spawnTimer -= dt;
    if (spawnTimer <= 0) {
        spawnEnemy();
        spawnTimer = qMax(0.35, 1.15 - wave * 0.06);
    }

    for (int i = 0; i < enemies.size(); ++i) {
        Entity& e = enemies[i];
        if (!e.alive)
            continue;
        e.rect.translate(e.vel.x() * dt, e.vel.y() * dt);
        e.frame++;
        // bounce sides
        if (e.rect.left() < pf.left() || e.rect.right() > pf.right())
            e.vel.setX(-e.vel.x());
        // hit player
        if (invulnTimer <= 0 && player.alive && e.rect.intersects(player.rect)) {
            e.alive = false;
            lives--;
            invulnTimer = 2.0;
            if (lives <= 0) {
                state = StateGameOver;
                if (score > highScore) {
                    highScore = score;
                    QSettings s;
                    s.setValue(QStringLiteral("arcade/shibeBlaster/highScore"), highScore);
                }
            }
        }
        // off bottom
        if (e.rect.top() > pf.bottom()) {
            e.alive = false;
            // leaked enemy costs a life lightly
            if (invulnTimer <= 0) {
                lives--;
                invulnTimer = 1.0;
                if (lives <= 0) {
                    state = StateGameOver;
                    if (score > highScore) {
                        highScore = score;
                        QSettings s;
                        s.setValue(QStringLiteral("arcade/shibeBlaster/highScore"), highScore);
                    }
                }
            }
        }
    }

    // collisions bullets vs enemies
    for (int bi = 0; bi < bullets.size(); ++bi) {
        Entity& b = bullets[bi];
        if (!b.alive || b.kind != 1)
            continue;
        for (int ei = 0; ei < enemies.size(); ++ei) {
            Entity& e = enemies[ei];
            if (!e.alive)
                continue;
            if (b.rect.intersects(e.rect)) {
                e.hp--;
                b.alive = false;
                if (e.hp <= 0) {
                    e.alive = false;
                    score += (e.kind == 3) ? 250 : 100;
                    if (score > 0 && score % 2000 == 0)
                        wave++;
                }
                break;
            }
        }
    }

    // compact dead
    QVector<Entity> nb, ne;
    for (int i = 0; i < bullets.size(); ++i)
        if (bullets[i].alive)
            nb.append(bullets[i]);
    for (int i = 0; i < enemies.size(); ++i)
        if (enemies[i].alive)
            ne.append(enemies[i]);
    bullets = nb;
    enemies = ne;
}

void ArcadeGameWidget::drawBackground(QPainter& p)
{
    const QRectF pf = playfield();
    // deep space
    QLinearGradient g(pf.topLeft(), pf.bottomLeft());
    g.setColorAt(0, QColor(8, 6, 28));
    g.setColorAt(1, QColor(20, 8, 40));
    p.fillRect(pf, g);

    // stars
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 60; ++i) {
        const double x = pf.left() + std::fmod(i * 97.3 + starPhase * (20 + i % 5), pf.width());
        const double y = pf.top() + std::fmod(i * 53.1 + starPhase * (30 + i % 7), pf.height());
        p.setBrush(QColor(255, 255, 255, 120 + (i % 3) * 40));
        p.drawEllipse(QPointF(x, y), 1.2, 1.2);
    }

    // moon
    p.setBrush(QColor(220, 220, 230));
    p.setPen(QPen(QColor(160, 160, 180), 2));
    const QPointF moon(pf.center().x(), pf.top() + 70);
    p.drawEllipse(moon, 48, 48);
    p.setBrush(QColor(180, 180, 195));
    p.setPen(Qt::NoPen);
    p.drawEllipse(moon + QPointF(-12, -8), 8, 7);
    p.drawEllipse(moon + QPointF(14, 6), 6, 5);

    // neon bezel
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 255, 200), 3));
    p.drawRoundedRect(pf.adjusted(1, 1, -1, -1), 8, 8);
    p.setPen(QPen(QColor(255, 0, 200, 160), 1));
    p.drawRoundedRect(pf.adjusted(5, 5, -5, -5), 6, 6);
}

void ArcadeGameWidget::drawPlayer(QPainter& p, const Entity& e)
{
    if (invulnTimer > 0 && ((int)(invulnTimer * 10) % 2 == 0))
        return; // blink

    const QRectF r = e.rect;
    // saucer
    p.setPen(QPen(QColor(20, 20, 40), 1));
    p.setBrush(QColor(180, 200, 220));
    p.drawEllipse(QRectF(r.left(), r.center().y() - 4, r.width(), r.height() * 0.55));
    p.setBrush(QColor(90, 180, 255, 180));
    p.drawEllipse(QRectF(r.left() + 8, r.top() + 2, r.width() - 16, r.height() * 0.45));
    // doge head
    p.setBrush(QColor(232, 176, 68));
    p.drawEllipse(QRectF(r.center().x() - 10, r.top() - 2, 20, 18));
    p.setBrush(QColor(40, 30, 20));
    p.drawEllipse(QRectF(r.center().x() - 5, r.top() + 4, 3, 3));
    p.drawEllipse(QRectF(r.center().x() + 3, r.top() + 4, 3, 3));
    // goggles
    p.setPen(QPen(QColor(0, 255, 180), 2));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(r.center().x() - 7, r.top() + 3, 7, 6));
    p.drawEllipse(QRectF(r.center().x() + 1, r.top() + 3, 7, 6));
    // thruster
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 140, 40, 200));
    p.drawEllipse(QRectF(r.center().x() - 5, r.bottom() - 4, 10, 8));
}

void ArcadeGameWidget::drawEnemy(QPainter& p, const Entity& e)
{
    const QRectF r = e.rect;
    if (e.kind == 3) {
        // FUD blob
        p.setPen(QPen(QColor(80, 20, 120), 2));
        p.setBrush(QColor(160, 60, 220));
        p.drawRoundedRect(r, 8, 8);
        p.setPen(QPen(QColor(255, 255, 100), 1));
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(10);
        p.setFont(f);
        p.drawText(r, Qt::AlignCenter, QStringLiteral("FUD"));
    } else {
        // Hater ghost
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(200, 60, 120));
        p.drawEllipse(r.adjusted(2, 2, -2, -8));
        p.drawRect(QRectF(r.left() + 2, r.center().y(), r.width() - 4, r.height() * 0.4));
        p.setBrush(QColor(20, 10, 20));
        p.drawEllipse(QRectF(r.left() + 6, r.top() + 8, 5, 5));
        p.drawEllipse(QRectF(r.right() - 12, r.top() + 8, 5, 5));
        p.setPen(QPen(QColor(255, 80, 80), 1));
        QFont f = font();
        f.setPixelSize(8);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRectF(r.left(), r.bottom() - 12, r.width(), 12), Qt::AlignCenter, QStringLiteral("HATER"));
    }
}

void ArcadeGameWidget::drawBullet(QPainter& p, const Entity& e)
{
    if (e.kind == 4) {
        p.setPen(QPen(QColor(255, 255, 80), 1));
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(9);
        p.setFont(f);
        p.drawText(e.rect, Qt::AlignCenter, QStringLiteral("WOW"));
        return;
    }
    p.setPen(Qt::NoPen);
    QLinearGradient g(e.rect.bottomLeft(), e.rect.topLeft());
    g.setColorAt(0, QColor(255, 80, 200));
    g.setColorAt(1, QColor(80, 255, 255));
    p.setBrush(g);
    p.drawRoundedRect(e.rect, 2, 2);
    p.setPen(QPen(QColor(255, 255, 120), 1));
    QFont f = font();
    f.setPixelSize(7);
    f.setBold(true);
    p.setFont(f);
    p.drawText(e.rect.adjusted(-8, -10, 8, 0), Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("WOW"));
}

void ArcadeGameWidget::drawHud(QPainter& p)
{
    const QRectF pf = playfield();
    QFont f = font();
    f.setFamily(QStringLiteral("Courier New"));
    f.setBold(true);
    f.setPixelSize(13);
    p.setFont(f);
    p.setPen(QColor(255, 230, 80));
    p.drawText(QRectF(pf.left() + 10, pf.top() + 8, 200, 18),
               Qt::AlignLeft, tr("MUCH SCORE: %1").arg(score));
    p.setPen(QColor(120, 255, 220));
    p.drawText(QRectF(pf.right() - 160, pf.top() + 8, 150, 18),
               Qt::AlignRight, tr("LIVES: %1").arg(lives));
    p.setPen(QColor(255, 120, 255));
    p.drawText(QRectF(pf.left() + 10, pf.bottom() - 22, 180, 16),
               Qt::AlignLeft, tr("CREDITS: %1  WAVE %2").arg(credits).arg(wave));
    p.setPen(QColor(180, 180, 255));
    p.drawText(QRectF(pf.right() - 200, pf.bottom() - 22, 190, 16),
               Qt::AlignRight, tr("HI: %1").arg(highScore));
}

void ArcadeGameWidget::drawTitle(QPainter& p)
{
    const QRectF pf = playfield();
    drawBackground(p);

    if (!titleArt.isNull()) {
        QPixmap scaled = titleArt.scaled(pf.size().toSize() * 0.92, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        const QPointF pos(pf.center().x() - scaled.width() / 2.0,
                          pf.center().y() - scaled.height() / 2.0 - 10);
        p.setOpacity(0.95);
        p.drawPixmap(pos, scaled);
        p.setOpacity(1.0);
    } else {
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(28);
        p.setFont(f);
        p.setPen(QColor(0, 255, 255));
        p.drawText(pf.adjusted(0, -40, 0, 0), Qt::AlignCenter, QStringLiteral("RETR-DOGE"));
        p.setPen(QColor(255, 0, 255));
        p.drawText(pf.adjusted(0, 10, 0, 0), Qt::AlignCenter, QStringLiteral("SHIBE BLASTER"));
        f.setPixelSize(14);
        p.setFont(f);
        p.setPen(QColor(255, 230, 80));
        p.drawText(pf.adjusted(0, 60, 0, 0), Qt::AlignCenter, QStringLiteral("MUCH WOW. VERY SHOOT."));
    }

    QFont f = font();
    f.setFamily(QStringLiteral("Courier New"));
    f.setBold(true);
    f.setPixelSize(14);
    p.setFont(f);
    const bool blink = ((int)(starPhase * 2) % 2) == 0;
    if (blink) {
        p.setPen(QColor(80, 255, 120));
        p.drawText(QRectF(pf.left(), pf.bottom() - 48, pf.width(), 20),
                   Qt::AlignCenter, tr("PRESS ENTER / SPACE — INSERT COIN"));
    }
    f.setPixelSize(11);
    p.setFont(f);
    p.setPen(QColor(180, 180, 220));
    p.drawText(QRectF(pf.left(), pf.bottom() - 28, pf.width(), 16),
               Qt::AlignCenter, tr("← → move   SPACE shoot   P pause   R restart"));
}

void ArcadeGameWidget::drawGameOver(QPainter& p)
{
    drawBackground(p);
    for (int i = 0; i < enemies.size(); ++i)
        drawEnemy(p, enemies[i]);
    drawPlayer(p, player);
    drawHud(p);

    p.fillRect(playfield(), QColor(0, 0, 0, 140));
    QFont f = font();
    f.setBold(true);
    f.setPixelSize(32);
    p.setFont(f);
    p.setPen(QColor(255, 60, 120));
    p.drawText(playfield(), Qt::AlignCenter, tr("GAME OVER\nMUCH SAD"));
    f.setPixelSize(14);
    p.setFont(f);
    p.setPen(QColor(255, 255, 120));
    p.drawText(playfield().adjusted(0, 80, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
               tr("SCORE %1   HI %2\nPRESS ENTER TO PLAY AGAIN").arg(score).arg(highScore));
}

void ArcadeGameWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor(5, 5, 12));

    if (state == StateTitle) {
        drawTitle(p);
        return;
    }
    if (state == StateGameOver) {
        drawGameOver(p);
        return;
    }

    drawBackground(p);
    for (int i = 0; i < enemies.size(); ++i)
        drawEnemy(p, enemies[i]);
    for (int i = 0; i < bullets.size(); ++i)
        drawBullet(p, bullets[i]);
    drawPlayer(p, player);
    drawHud(p);

    if (state == StatePaused) {
        p.fillRect(playfield(), QColor(0, 0, 0, 120));
        QFont f = font();
        f.setBold(true);
        f.setPixelSize(28);
        p.setFont(f);
        p.setPen(QColor(0, 255, 255));
        p.drawText(playfield(), Qt::AlignCenter, tr("PAUSED\nP TO RESUME"));
    }
}

void ArcadeGameWidget::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
        keyLeft = true;
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        keyRight = true;
        break;
    case Qt::Key_Space:
        keyFire = true;
        if (state == StateTitle || state == StateGameOver)
            startNewGame();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (state == StateTitle || state == StateGameOver)
            startNewGame();
        break;
    case Qt::Key_P:
        if (state == StatePlaying)
            pauseGame();
        else if (state == StatePaused)
            resumeGame();
        break;
    case Qt::Key_R:
        startNewGame();
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

void ArcadeGameWidget::keyReleaseEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_A:
        keyLeft = false;
        break;
    case Qt::Key_Right:
    case Qt::Key_D:
        keyRight = false;
        break;
    case Qt::Key_Space:
        keyFire = false;
        break;
    default:
        QWidget::keyReleaseEvent(event);
        return;
    }
    event->accept();
}

void ArcadeGameWidget::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
}

void ArcadeGameWidget::focusOutEvent(QFocusEvent* event)
{
    keyLeft = keyRight = keyFire = false;
    if (state == StatePlaying)
        pauseGame();
    QWidget::focusOutEvent(event);
}

void ArcadeGameWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // keep player in bounds
    const QRectF pf = playfield();
    if (player.rect.left() < pf.left())
        player.rect.moveLeft(pf.left());
    if (player.rect.right() > pf.right())
        player.rect.moveRight(pf.right());
}
