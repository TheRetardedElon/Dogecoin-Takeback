// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "moderncard.h"
#include "thememanager.h"

#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QIcon>
#include <QDebug>

ModernCard::ModernCard(QWidget* parent, CardType type)
    : QFrame(parent)
    , m_cardType(type)
    , m_elevation(2)
    , m_roundedCorners(true)
    , m_hoverEffect(true)
    , m_shadowBlurRadius(8.0)
    , m_shadowOffsetX(0.0)
    , m_shadowOffsetY(2.0)
    , m_shadowEffect(nullptr)
    , m_hoverAnimation(nullptr)
{
    setupUI();
    updateStyle();
}

ModernCard::~ModernCard()
{
}

void ModernCard::setupUI()
{
    // Main layout
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(16, 16, 16, 16);
    m_mainLayout->setSpacing(12);
    
    // Header layout
    m_headerLayout = new QHBoxLayout();
    m_headerLayout->setSpacing(8);
    
    // Icon label
    m_iconLabel = new QLabel();
    m_iconLabel->setFixedSize(24, 24);
    m_iconLabel->setScaledContents(true);
    m_headerLayout->addWidget(m_iconLabel);
    
    // Title label
    m_titleLabel = new QLabel();
    m_titleLabel->setStyleSheet("font-weight: 600; font-size: 14px;");
    m_headerLayout->addWidget(m_titleLabel);
    
    m_headerLayout->addStretch();
    
    m_mainLayout->addLayout(m_headerLayout);
    
    // Content widget (will be set later)
    m_contentWidget = nullptr;
    
    // Shadow effect
    m_shadowEffect = new QGraphicsDropShadowEffect();
    m_shadowEffect->setBlurRadius(m_shadowBlurRadius);
    m_shadowEffect->setOffset(m_shadowOffsetX, m_shadowOffsetY);
    setGraphicsEffect(m_shadowEffect);
    
    // Hover animation
    m_hoverAnimation = new QPropertyAnimation(this, "shadowBlurRadius");
    m_hoverAnimation->setDuration(200);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void ModernCard::setTitle(const QString& title)
{
    m_titleLabel->setText(title);
}

void ModernCard::setContent(QWidget* content)
{
    if (m_contentWidget) {
        m_mainLayout->removeWidget(m_contentWidget);
        m_contentWidget->deleteLater();
    }
    
    m_contentWidget = content;
    if (m_contentWidget) {
        m_mainLayout->addWidget(m_contentWidget);
    }
}

void ModernCard::setIcon(const QIcon& icon)
{
    m_iconLabel->setPixmap(icon.pixmap(24, 24));
}

void ModernCard::setCardType(CardType type)
{
    m_cardType = type;
    updateStyle();
}

void ModernCard::setElevation(int elevation)
{
    m_elevation = elevation;
    updateStyle();
}

void ModernCard::setRoundedCorners(bool rounded)
{
    m_roundedCorners = rounded;
    updateStyle();
}

void ModernCard::setHoverEffect(bool enabled)
{
    m_hoverEffect = enabled;
}

void ModernCard::setShadowBlurRadius(qreal radius)
{
    m_shadowBlurRadius = radius;
    if (m_shadowEffect) {
        m_shadowEffect->setBlurRadius(radius);
    }
}

void ModernCard::setShadowOffsetX(qreal offset)
{
    m_shadowOffsetX = offset;
    if (m_shadowEffect) {
        m_shadowEffect->setOffset(offset, m_shadowOffsetY);
    }
}

void ModernCard::setShadowOffsetY(qreal offset)
{
    m_shadowOffsetY = offset;
    if (m_shadowEffect) {
        m_shadowEffect->setOffset(m_shadowOffsetX, offset);
    }
}

void ModernCard::enterEvent(QEvent* event)
{
    QFrame::enterEvent(event);
    if (m_hoverEffect) {
        animateHover(true);
    }
}

void ModernCard::leaveEvent(QEvent* event)
{
    QFrame::leaveEvent(event);
    if (m_hoverEffect) {
        animateHover(false);
    }
}

void ModernCard::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect().adjusted(1, 1, -1, -1);
    
    // Get theme colors
    ThemeManager* themeManager = ThemeManager::instance();
    QColor backgroundColor = themeManager->color("cardBackground");
    QColor borderColor = themeManager->color("cardBorder");
    
    // Adjust colors based on card type
    switch (m_cardType) {
        case Primary:
            backgroundColor = themeManager->color("primaryAccent");
            break;
        case Success:
            backgroundColor = themeManager->color("successColor");
            break;
        case Warning:
            backgroundColor = themeManager->color("warningColor");
            break;
        case Error:
            backgroundColor = themeManager->color("errorColor");
            break;
        case Info:
            backgroundColor = themeManager->color("infoColor");
            break;
        default:
            break;
    }
    
    // Draw background
    painter.setBrush(QBrush(backgroundColor));
    painter.setPen(QPen(borderColor, 1));
    
    if (m_roundedCorners) {
        painter.drawRoundedRect(rect, 8, 8);
    } else {
        painter.drawRect(rect);
    }
}

void ModernCard::updateStyle()
{
    // Update shadow effect
    if (m_shadowEffect) {
        ThemeManager* themeManager = ThemeManager::instance();
        QColor shadowColor = themeManager->color("shadowColor");
        m_shadowEffect->setColor(shadowColor);
        m_shadowEffect->setBlurRadius(m_shadowBlurRadius);
        m_shadowEffect->setOffset(m_shadowOffsetX, m_shadowOffsetY);
    }
    
    // Update frame style
    setFrameStyle(QFrame::NoFrame);
    
    // Apply theme stylesheet
    ThemeManager::instance()->applyTheme(this);
}

void ModernCard::animateHover(bool hover)
{
    if (!m_hoverAnimation) return;
    
    qreal targetRadius = hover ? m_shadowBlurRadius * 1.5 : m_shadowBlurRadius;
    
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_shadowBlurRadius);
    m_hoverAnimation->setEndValue(targetRadius);
    m_hoverAnimation->start();
}
