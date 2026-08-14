// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "arcadepage.h"

#include "arcadegamewidget.h"
#include "platformstyle.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

// GPE hosts the public arcade hub; Flappy lives on grok.me for now.
static const char* kArcadeHubUrl = "https://gopastearth.com/arcade";
static const char* kArcadeHubAlt = "https://arcade.gopastearth.com/";
static const char* kFlappyDogeUrl = "https://flappy-doge.grok.me/";

ArcadePage::ArcadePage(const PlatformStyle* _platformStyle, QWidget* parent)
    : QWidget(parent),
      platformStyle(_platformStyle),
      selectedIndex(0),
      titleLabel(0),
      subLabel(0),
      hubBtn(0),
      playBtn(0),
      categoryGroup(0),
      categoryBar(0),
      shelfScroll(0),
      shelfHost(0),
      stage(0),
      shelfPage(0),
      playPage(0),
      blaster(0),
      webTitle(0),
      webBlurb(0),
      webLaunchBtn(0),
      backToShelfBtn(0)
{
    activeCategory = QStringLiteral("All");
    rebuildCatalog();
    setupUi();
    rebuildShelf();
}

void ArcadePage::rebuildCatalog()
{
    catalog.clear();

    GameEntry flappy;
    flappy.id = QStringLiteral("flappy-doge");
    flappy.title = tr("Flappy Doge");
    flappy.category = QStringLiteral("Classic");
    flappy.blurb = tr("Much Fly. Very Wow. Tap/space to fly Doge through crypto towers. "
                      "Opens the live cabinet at flappy-doge.grok.me.");
    flappy.badge = tr("Featured");
    flappy.kind = GameWebFlappy;
    flappy.url = QString::fromUtf8(kFlappyDogeUrl);
    flappy.featured = true;
    catalog.push_back(flappy);

    GameEntry blasterEntry;
    blasterEntry.id = QStringLiteral("shibe-blaster");
    blasterEntry.title = tr("Retr-Doge Shibe Blaster");
    blasterEntry.category = QStringLiteral("Action");
    blasterEntry.blurb = tr("← → to fly, SPACE to blast HATERS & FUD. P pauses. "
                            "Pure local Qt cabinet — no browser, no wallet.");
    blasterEntry.badge = tr("Local");
    blasterEntry.kind = GameLocalBlaster;
    blasterEntry.featured = false;
    catalog.push_back(blasterEntry);

    GameEntry jump;
    jump.id = QStringLiteral("doge-jump");
    jump.title = tr("Doge Jump");
    jump.category = QStringLiteral("Classic");
    jump.blurb = tr("Bounce to the moon. Coming soon to the GPE arcade cabinet.");
    jump.badge = tr("Soon");
    jump.kind = GameComingSoon;
    jump.featured = false;
    catalog.push_back(jump);

    GameEntry racer;
    racer.id = QStringLiteral("doge-racer");
    racer.title = tr("Doge Racer");
    racer.category = QStringLiteral("Racing");
    racer.blurb = tr("High-speed shibe circuits. Coming soon.");
    racer.badge = tr("Soon");
    racer.kind = GameComingSoon;
    racer.featured = false;
    catalog.push_back(racer);

    GameEntry puzzle;
    puzzle.id = QStringLiteral("wow-puzzle");
    puzzle.title = tr("Wow Puzzle");
    puzzle.category = QStringLiteral("Puzzle");
    puzzle.blurb = tr("Very brain. Much solve. Coming soon.");
    puzzle.badge = tr("Soon");
    puzzle.kind = GameComingSoon;
    puzzle.featured = false;
    catalog.push_back(puzzle);
}

void ArcadePage::setupUi()
{
    setObjectName(QStringLiteral("arcadePage"));
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // Header
    QHBoxLayout* head = new QHBoxLayout();
    titleLabel = new QLabel(tr("Doge Arcade"));
    QFont tf = titleLabel->font();
    tf.setPointSize(tf.pointSize() + 4);
    tf.setBold(true);
    titleLabel->setFont(tf);
    head->addWidget(titleLabel);
    head->addStretch();

    hubBtn = new QPushButton(tr("Open GPE Hub"));
    hubBtn->setObjectName(QStringLiteral("homeQuickButton"));
    hubBtn->setToolTip(tr("Open arcade.gopastearth.com / gopastearth.com/arcade in your browser"));
    head->addWidget(hubBtn);

    playBtn = new QPushButton(tr("Play selected"));
    playBtn->setObjectName(QStringLiteral("homeQuickButton"));
    playBtn->setToolTip(tr("Launch the selected game in-cabinet or in the browser"));
    head->addWidget(playBtn);
    root->addLayout(head);

    subLabel = new QLabel(tr("Multi-game cabinet — local blaster + live web titles. "
                             "Client fun only; never touches your wallet or node."));
    subLabel->setObjectName(QStringLiteral("mutedLabel"));
    subLabel->setWordWrap(true);
    root->addWidget(subLabel);

    // Category chips
    categoryBar = new QWidget(this);
    QHBoxLayout* catLay = new QHBoxLayout(categoryBar);
    catLay->setContentsMargins(0, 0, 0, 0);
    catLay->setSpacing(8);
    categoryGroup = new QButtonGroup(this);
    categoryGroup->setExclusive(true);

    const QStringList cats = QStringList()
        << QStringLiteral("All")
        << QStringLiteral("Classic")
        << QStringLiteral("Action")
        << QStringLiteral("Puzzle")
        << QStringLiteral("Racing");
    for (int i = 0; i < cats.size(); ++i) {
        QPushButton* chip = new QPushButton(cats[i], categoryBar);
        chip->setCheckable(true);
        chip->setObjectName(QStringLiteral("homeQuickButton"));
        chip->setCursor(Qt::PointingHandCursor);
        if (i == 0)
            chip->setChecked(true);
        categoryGroup->addButton(chip, i);
        catLay->addWidget(chip);
    }
    catLay->addStretch();
    root->addWidget(categoryBar);
    connect(categoryGroup, SIGNAL(buttonClicked(int)), this, SLOT(onCategoryChanged(int)));

    // Stage: shelf grid vs in-cabinet play
    stage = new QStackedWidget(this);

    shelfPage = new QWidget(stage);
    QVBoxLayout* shelfLay = new QVBoxLayout(shelfPage);
    shelfLay->setContentsMargins(0, 0, 0, 0);
    shelfScroll = new QScrollArea(shelfPage);
    shelfScroll->setWidgetResizable(true);
    shelfScroll->setFrameShape(QFrame::NoFrame);
    shelfHost = new QWidget;
    shelfHost->setObjectName(QStringLiteral("arcadeShelfHost"));
    shelfScroll->setWidget(shelfHost);
    shelfLay->addWidget(shelfScroll);
    stage->addWidget(shelfPage);

    playPage = new QWidget(stage);
    QVBoxLayout* playLay = new QVBoxLayout(playPage);
    playLay->setContentsMargins(0, 0, 0, 0);
    playLay->setSpacing(8);

    QHBoxLayout* playHead = new QHBoxLayout();
    backToShelfBtn = new QPushButton(tr("← Cabinet"), playPage);
    backToShelfBtn->setObjectName(QStringLiteral("homeQuickButton"));
    playHead->addWidget(backToShelfBtn);
    playHead->addStretch();
    playLay->addLayout(playHead);

    blaster = new ArcadeGameWidget(playPage);
    blaster->setMinimumHeight(420);
    playLay->addWidget(blaster, 1);

    webTitle = new QLabel(playPage);
    QFont wf = webTitle->font();
    wf.setPointSize(wf.pointSize() + 2);
    wf.setBold(true);
    webTitle->setFont(wf);
    webTitle->setVisible(false);
    playLay->addWidget(webTitle);

    webBlurb = new QLabel(playPage);
    webBlurb->setObjectName(QStringLiteral("mutedLabel"));
    webBlurb->setWordWrap(true);
    webBlurb->setVisible(false);
    playLay->addWidget(webBlurb);

    webLaunchBtn = new QPushButton(tr("Launch in browser"), playPage);
    webLaunchBtn->setObjectName(QStringLiteral("homeQuickButton"));
    webLaunchBtn->setVisible(false);
    playLay->addWidget(webLaunchBtn, 0, Qt::AlignLeft);
    playLay->addStretch();

    stage->addWidget(playPage);
    root->addWidget(stage, 1);

    connect(hubBtn, SIGNAL(clicked()), this, SLOT(onOpenHubClicked()));
    connect(playBtn, SIGNAL(clicked()), this, SLOT(onPlayClicked()));
    connect(webLaunchBtn, SIGNAL(clicked()), this, SLOT(onLaunchSelected()));
    connect(backToShelfBtn, &QPushButton::clicked, this, [this]() {
        if (stage)
            stage->setCurrentWidget(shelfPage);
        if (blaster)
            blaster->pauseGame();
    });
}

QFrame* ArcadePage::makeGameCard(int index, const GameEntry& g)
{
    QFrame* card = new QFrame(shelfHost);
    card->setObjectName(QStringLiteral("arcadeGameCard"));
    card->setFrameShape(QFrame::StyledPanel);
    card->setMinimumWidth(200);
    card->setMaximumWidth(280);
    card->setCursor(Qt::PointingHandCursor);
    card->setStyleSheet(QStringLiteral(
        "QFrame#arcadeGameCard {"
        "  border: 1px solid rgba(255,255,255,0.12);"
        "  border-radius: 12px;"
        "  background: rgba(30, 34, 48, 0.92);"
        "  padding: 8px;"
        "}"
        "QFrame#arcadeGameCard:hover {"
        "  border: 1px solid rgba(255, 204, 0, 0.55);"
        "}"));

    QVBoxLayout* lay = new QVBoxLayout(card);
    lay->setSpacing(6);

    QLabel* badge = new QLabel(g.badge, card);
    badge->setStyleSheet(QStringLiteral(
        "QLabel { color: #1a1a1a; background: #ffcc00; border-radius: 6px; "
        "padding: 2px 8px; font-weight: bold; font-size: 11px; }"));
    badge->setAlignment(Qt::AlignCenter);
    badge->setMaximumWidth(90);
    lay->addWidget(badge, 0, Qt::AlignLeft);

    // Pixel-ish art placeholder
    QLabel* art = new QLabel(card);
    art->setFixedHeight(88);
    art->setAlignment(Qt::AlignCenter);
    QString emoji = QStringLiteral("🎮");
    if (g.kind == GameWebFlappy)
        emoji = QStringLiteral("🐕");
    else if (g.kind == GameLocalBlaster)
        emoji = QStringLiteral("🚀");
    else if (g.category == QStringLiteral("Racing"))
        emoji = QStringLiteral("🏎️");
    else if (g.category == QStringLiteral("Puzzle"))
        emoji = QStringLiteral("🧩");
    art->setText(QStringLiteral("<span style='font-size:42px'>%1</span>").arg(emoji));
    art->setStyleSheet(QStringLiteral(
        "QLabel { background: qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        " stop:0 #1b2744, stop:1 #0d1528); border-radius: 8px; }"));
    lay->addWidget(art);

    QLabel* name = new QLabel(g.title, card);
    QFont nf = name->font();
    nf.setBold(true);
    name->setFont(nf);
    name->setWordWrap(true);
    lay->addWidget(name);

    QLabel* cat = new QLabel(g.category, card);
    cat->setObjectName(QStringLiteral("mutedLabel"));
    lay->addWidget(cat);

    QLabel* desc = new QLabel(g.blurb, card);
    desc->setObjectName(QStringLiteral("mutedLabel"));
    desc->setWordWrap(true);
    desc->setMaximumHeight(54);
    lay->addWidget(desc);

    QPushButton* go = new QPushButton(
        g.kind == GameComingSoon ? tr("Coming soon") :
        (g.kind == GameWebFlappy ? tr("Play Now") : tr("Launch in Cabinet")),
        card);
    go->setObjectName(QStringLiteral("homeQuickButton"));
    go->setEnabled(g.kind != GameComingSoon);
    lay->addWidget(go);

    const int idx = index;
    connect(go, &QPushButton::clicked, this, [this, idx]() {
        selectedIndex = idx;
        onGameSelected(idx);
    });

    return card;
}

void ArcadePage::rebuildShelf()
{
    if (!shelfHost)
        return;

    // Clear previous grid
    if (QLayout* old = shelfHost->layout()) {
        QLayoutItem* child;
        while ((child = old->takeAt(0)) != 0) {
            if (child->widget())
                child->widget()->deleteLater();
            delete child;
        }
        delete old;
    }

    QGridLayout* grid = new QGridLayout(shelfHost);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(12);
    grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    int col = 0;
    int row = 0;
    const int cols = 3;
    for (int i = 0; i < catalog.size(); ++i) {
        const GameEntry& g = catalog[i];
        if (activeCategory != QStringLiteral("All") && g.category != activeCategory)
            continue;
        QFrame* card = makeGameCard(i, g);
        grid->addWidget(card, row, col);
        col++;
        if (col >= cols) {
            col = 0;
            row++;
        }
    }
    grid->setColumnStretch(cols, 1);
    grid->setRowStretch(row + 1, 1);
}

void ArcadePage::showCabinetFor(int index)
{
    if (index < 0 || index >= catalog.size())
        return;
    selectedIndex = index;
    const GameEntry& g = catalog[index];

    if (g.kind == GameComingSoon)
        return;

    if (g.kind == GameLocalBlaster) {
        if (blaster) {
            blaster->setVisible(true);
            blaster->startNewGame();
            blaster->setFocus();
        }
        if (webTitle) webTitle->setVisible(false);
        if (webBlurb) webBlurb->setVisible(false);
        if (webLaunchBtn) webLaunchBtn->setVisible(false);
        if (stage) stage->setCurrentWidget(playPage);
        return;
    }

    // Web / Flappy — detail pane + browser launch (no QWebEngine in Core Pro)
    if (blaster) {
        blaster->pauseGame();
        blaster->setVisible(false);
    }
    if (webTitle) {
        webTitle->setText(g.title);
        webTitle->setVisible(true);
    }
    if (webBlurb) {
        webBlurb->setText(g.blurb + QStringLiteral("\n\n") +
                          tr("Core Pro opens web games in your system browser "
                             "(no embedded WebEngine). GPE can also host these "
                             "in the full arcade cabinet."));
        webBlurb->setVisible(true);
    }
    if (webLaunchBtn) {
        webLaunchBtn->setText(tr("Launch %1").arg(g.title));
        webLaunchBtn->setVisible(true);
    }
    if (stage) stage->setCurrentWidget(playPage);
}

void ArcadePage::focusGame()
{
    // Returning to Arcade tab → show shelf (cabinet overview)
    if (stage)
        stage->setCurrentWidget(shelfPage);
    if (blaster)
        blaster->pauseGame();
}

void ArcadePage::onPlayClicked()
{
    if (selectedIndex >= 0 && selectedIndex < catalog.size())
        showCabinetFor(selectedIndex);
    else if (!catalog.isEmpty())
        showCabinetFor(0);
}

void ArcadePage::onOpenHubClicked()
{
    // Prefer the SPA route that currently serves the GPE arcade shell.
    // arcade.gopastearth.com root may 404 depending on CDN config; /arcade works.
    if (!QDesktopServices::openUrl(QUrl(QString::fromUtf8(kArcadeHubUrl)))) {
        QDesktopServices::openUrl(QUrl(QString::fromUtf8(kArcadeHubAlt)));
    }
}

void ArcadePage::onCategoryChanged(int id)
{
    QAbstractButton* b = categoryGroup ? categoryGroup->button(id) : 0;
    if (!b)
        return;
    activeCategory = b->text();
    rebuildShelf();
}

void ArcadePage::onGameSelected(int index)
{
    showCabinetFor(index);
}

void ArcadePage::onLaunchSelected()
{
    if (selectedIndex < 0 || selectedIndex >= catalog.size())
        return;
    const GameEntry& g = catalog[selectedIndex];
    if (g.kind == GameWebFlappy || (g.kind != GameLocalBlaster && !g.url.isEmpty())) {
        QDesktopServices::openUrl(QUrl(g.url));
        return;
    }
    if (g.kind == GameLocalBlaster)
        showCabinetFor(selectedIndex);
}
