// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MODERNCARD_H
#define BITCOIN_QT_MODERNCARD_H

#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QWidget>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>

class ModernCard : public QFrame
{
    Q_OBJECT
    Q_PROPERTY(qreal shadowBlurRadius READ shadowBlurRadius WRITE setShadowBlurRadius)
    Q_PROPERTY(qreal shadowOffsetX READ shadowOffsetX WRITE setShadowOffsetX)
    Q_PROPERTY(qreal shadowOffsetY READ shadowOffsetY WRITE setShadowOffsetY)

public:
    enum CardType {
        Default,
        Primary,
        Success,
        Warning,
        Error,
        Info
    };

    explicit ModernCard(QWidget* parent = nullptr, CardType type = Default);
    ~ModernCard();

    // Card content
    void setTitle(const QString& title);
    void setContent(QWidget* content);
    void setIcon(const QIcon& icon);
    void setCardType(CardType type);
    
    // Styling
    void setElevation(int elevation);
    void setRoundedCorners(bool rounded);
    void setHoverEffect(bool enabled);
    
    // Shadow properties
    qreal shadowBlurRadius() const { return m_shadowBlurRadius; }
    void setShadowBlurRadius(qreal radius);
    qreal shadowOffsetX() const { return m_shadowOffsetX; }
    void setShadowOffsetX(qreal offset);
    qreal shadowOffsetY() const { return m_shadowOffsetY; }
    void setShadowOffsetY(qreal offset);

protected:
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

public:
    void updateStyle();

private:
    void setupUI();
    void animateHover(bool hover);

    QVBoxLayout* m_mainLayout;
    QHBoxLayout* m_headerLayout;
    QLabel* m_titleLabel;
    QLabel* m_iconLabel;
    QWidget* m_contentWidget;
    
    CardType m_cardType;
    int m_elevation;
    bool m_roundedCorners;
    bool m_hoverEffect;
    
    // Shadow properties
    qreal m_shadowBlurRadius;
    qreal m_shadowOffsetX;
    qreal m_shadowOffsetY;
    
    QGraphicsDropShadowEffect* m_shadowEffect;
    QPropertyAnimation* m_hoverAnimation;
};

#endif // BITCOIN_QT_MODERNCARD_H
