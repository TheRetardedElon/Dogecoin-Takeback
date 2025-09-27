// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "modernnavigation.h"
#include "thememanager.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QFont>
#include <QDebug>

ModernNavigation::ModernNavigation(QWidget* parent)
    : QWidget(parent)
    , m_currentItem(Overview)
    , m_compactMode(false)
    , m_animation(nullptr)
{
    setupUI();
    applyTheme();
    
    // Connect theme changes
    connect(ThemeManager::instance(), &ThemeManager::colorsChanged, this, &ModernNavigation::applyTheme);
}

ModernNavigation::~ModernNavigation()
{
}

void ModernNavigation::setupUI()
{
    // Set modern sleek styling
    setStyleSheet(R"(
        ModernNavigation {
            background-color: #2d2d2d;
            border-right: 1px solid #404040;
        }
        QPushButton {
            background-color: transparent;
            border: none;
            padding: 12px 16px;
            margin: 2px;
            text-align: left;
            color: #cccccc;
            font-size: 14px;
            font-weight: 500;
            border-radius: 6px;
            min-height: 20px;
        }
        QPushButton:hover {
            background-color: #3d3d3d;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #4d4d4d;
        }
        QLabel {
            color: #ffffff;
        }
    )");
    
    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);
    
    // Header section
    m_headerLayout = new QHBoxLayout();
    m_headerLayout->setSpacing(8);
    
    // Logo
    m_logoLabel = new QLabel("Ð");
    m_logoLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #007bff;");
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_headerLayout->addWidget(m_logoLabel);
    
    // Toggle button
    m_toggleButton = new QPushButton("☰");
    m_toggleButton->setFixedSize(32, 32);
    m_toggleButton->setStyleSheet(
        "QPushButton {"
        "    background-color: transparent;"
        "    border: none;"
        "    border-radius: 4px;"
        "    font-size: 16px;"
        "}"
        "QPushButton:hover {"
        "    background-color: rgba(0, 123, 255, 0.1);"
        "}"
    );
    connect(m_toggleButton, &QPushButton::clicked, this, &ModernNavigation::onToggleClicked);
    m_headerLayout->addWidget(m_toggleButton);
    
    m_headerLayout->addStretch();
    m_mainLayout->addLayout(m_headerLayout);
    
    // Navigation frame
    m_navFrame = new QFrame();
    m_navFrame->setFrameStyle(QFrame::NoFrame);
    m_navLayout = new QVBoxLayout(m_navFrame);
    m_navLayout->setContentsMargins(0, 0, 0, 0);
    m_navLayout->setSpacing(4);
    
    // Navigation buttons
    m_overviewButton = new QPushButton("📊 Overview");
    m_sendButton = new QPushButton("📤 Send");
    m_receiveButton = new QPushButton("📥 Receive");
    m_historyButton = new QPushButton("📋 History");
    m_addressBookButton = new QPushButton("👥 Address Book");
    m_consoleButton = new QPushButton("💻 Console");
    // Store buttons in map
    m_navButtons[Overview] = m_overviewButton;
    m_navButtons[Send] = m_sendButton;
    m_navButtons[Receive] = m_receiveButton;
    m_navButtons[History] = m_historyButton;
    m_navButtons[AddressBook] = m_addressBookButton;
    m_navButtons[Console] = m_consoleButton;
    
    // Add buttons to layout (Settings button removed - accessible via main menu)
    m_navLayout->addWidget(m_overviewButton);
    m_navLayout->addWidget(m_sendButton);
    m_navLayout->addWidget(m_receiveButton);
    m_navLayout->addWidget(m_historyButton);
    m_navLayout->addWidget(m_addressBookButton);
    m_navLayout->addWidget(m_consoleButton);
    
    m_navLayout->addStretch();
    m_mainLayout->addWidget(m_navFrame);
    
    // Status section
    m_statusFrame = new QFrame();
    m_statusFrame->setFrameStyle(QFrame::StyledPanel);
    m_statusLayout = new QVBoxLayout(m_statusFrame);
    m_statusLayout->setContentsMargins(8, 8, 8, 8);
    m_statusLayout->setSpacing(4);
    
    m_balanceLabel = new QLabel("0.00000000 DOGE");
    m_balanceLabel->setStyleSheet("font-size: 14px; font-weight: 600;");
    m_balanceLabel->setAlignment(Qt::AlignCenter);
    m_statusLayout->addWidget(m_balanceLabel);
    
    m_statusLabel = new QLabel("Synchronizing...");
    m_statusLabel->setStyleSheet("font-size: 12px; color: #6c757d;");
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLayout->addWidget(m_statusLabel);
    
    m_mainLayout->addWidget(m_statusFrame);
    
    // Connect button signals
    connect(m_overviewButton, &QPushButton::clicked, this, &ModernNavigation::onItemClicked);
    connect(m_sendButton, &QPushButton::clicked, this, &ModernNavigation::onItemClicked);
    connect(m_receiveButton, &QPushButton::clicked, this, &ModernNavigation::onItemClicked);
    connect(m_historyButton, &QPushButton::clicked, this, &ModernNavigation::onItemClicked);
    connect(m_addressBookButton, &QPushButton::clicked, this, &ModernNavigation::onItemClicked);
    connect(m_consoleButton, &QPushButton::clicked, this, &ModernNavigation::onItemClicked);
    // Settings button removed - no longer needed
    
    // Set initial state
    setCurrentItem(Overview);
}

void ModernNavigation::setCurrentItem(NavItem item)
{
    if (m_currentItem != item) {
        NavItem oldItem = m_currentItem;
        m_currentItem = item;
        updateItemStyles();
        animateItemChange(oldItem);
    }
}

void ModernNavigation::setCompactMode(bool compact)
{
    if (m_compactMode != compact) {
        m_compactMode = compact;
        
        // Update button text visibility
        for (auto button : m_navButtons) {
            QString text = button->text();
            if (compact) {
                // Show only emoji
                QString emoji = text.split(' ').first();
                button->setText(emoji);
                button->setFixedWidth(40);
            } else {
                // Show full text
                button->setFixedWidth(120);
            }
        }
        
        // Update logo visibility
        m_logoLabel->setVisible(!compact);
        
        Q_EMIT toggleCompact();
    }
}

void ModernNavigation::onItemClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    // Find which item was clicked
    for (auto it = m_navButtons.begin(); it != m_navButtons.end(); ++it) {
        if (it.value() == button) {
            setCurrentItem(it.key());
            Q_EMIT itemClicked(it.key());
            break;
        }
    }
}

void ModernNavigation::onToggleClicked()
{
    setCompactMode(!m_compactMode);
}

void ModernNavigation::updateItemStyles()
{
    ThemeManager* themeManager = ThemeManager::instance();
    QColor primaryAccent = themeManager->color("primaryAccent");
    QColor primaryText = themeManager->color("primaryText");
    QColor secondaryText = themeManager->color("secondaryText");
    QColor cardBackground = themeManager->color("cardBackground");
    
    QString activeStyle = QString(
        "QPushButton {"
        "    background-color: %1;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 12px 16px;"
        "    margin: 2px;"
        "    text-align: left;"
        "    font-weight: 600;"
        "    min-height: 20px;"
        "}"
    ).arg(primaryAccent.name());
    
    QString normalStyle = QString(
        "QPushButton {"
        "    background-color: transparent;"
        "    color: %1;"
        "    border: none;"
        "    border-radius: 6px;"
        "    padding: 12px 16px;"
        "    margin: 2px;"
        "    text-align: left;"
        "    font-weight: 500;"
        "    min-height: 20px;"
        "}"
        "QPushButton:hover {"
        "    background-color: %2;"
        "}"
    ).arg(primaryText.name()).arg(primaryAccent.name() + "20");
    
    // Apply styles to all buttons
    for (auto it = m_navButtons.begin(); it != m_navButtons.end(); ++it) {
        if (it.key() == m_currentItem) {
            it.value()->setStyleSheet(activeStyle);
        } else {
            it.value()->setStyleSheet(normalStyle);
        }
    }
}

void ModernNavigation::animateItemChange(NavItem newItem)
{
    // Simple animation - could be enhanced with more sophisticated effects
    if (m_animation) {
        m_animation->stop();
    }
    
    // Update styles immediately for now
    updateItemStyles();
}

void ModernNavigation::applyTheme()
{
    // Apply theme to the navigation widget
    ThemeManager::instance()->applyTheme(this);
    
    // Update status frame
    m_statusFrame->setStyleSheet(
        "QFrame {"
        "    background-color: " + ThemeManager::instance()->color("cardBackground").name() + ";"
        "    border: 1px solid " + ThemeManager::instance()->color("primaryBorder").name() + ";"
        "    border-radius: 8px;"
        "}"
    );
    
    // Update item styles
    updateItemStyles();
}
