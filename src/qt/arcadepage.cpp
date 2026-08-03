// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "arcadepage.h"

#include "arcadegamewidget.h"
#include "platformstyle.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ArcadePage::ArcadePage(const PlatformStyle* _platformStyle, QWidget* parent)
    : QWidget(parent),
      platformStyle(_platformStyle),
      game(0),
      playBtn(0)
{
    setupUi();
}

void ArcadePage::setupUi()
{
    setObjectName(QStringLiteral("arcadePage"));
    QVBoxLayout* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    QHBoxLayout* head = new QHBoxLayout();
    QLabel* title = new QLabel(tr("Arcade"));
    QFont tf = title->font();
    tf.setPointSize(tf.pointSize() + 4);
    tf.setBold(true);
    title->setFont(tf);
    head->addWidget(title);
    head->addStretch();

    playBtn = new QPushButton(tr("Play / Restart"));
    playBtn->setObjectName(QStringLiteral("homeQuickButton"));
    playBtn->setToolTip(tr("Start or restart Retr-Doge Shibe Blaster"));
    head->addWidget(playBtn);
    root->addLayout(head);

    QLabel* sub = new QLabel(tr("Retr-Doge Shibe Blaster — much wow, very shoot. "
                                "Use ← → to fly, SPACE to blast HATERS & FUD. "
                                "P pauses. Pure client fun — no wallet or network required."));
    sub->setObjectName(QStringLiteral("mutedLabel"));
    sub->setWordWrap(true);
    root->addWidget(sub);

    game = new ArcadeGameWidget(this);
    game->setMinimumHeight(420);
    root->addWidget(game, 1);

    connect(playBtn, SIGNAL(clicked()), this, SLOT(onPlayClicked()));
}

void ArcadePage::focusGame()
{
    if (game)
        game->setFocus();
}

void ArcadePage::onPlayClicked()
{
    if (!game)
        return;
    game->startNewGame();
    game->setFocus();
}
