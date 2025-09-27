# Dogecoin Core - Modern UI System

This document describes the new modern UI system implemented for Dogecoin Core, featuring themeable interfaces, dark mode support, and contemporary design elements.

## 🎨 Features

### Theme System
- **Light Theme**: Clean, modern light interface
- **Dark Theme**: Eye-friendly dark mode with proper contrast
- **Auto Theme**: Automatically follows system theme preferences
- **Custom Themes**: Create and save your own color schemes
- **Live Preview**: See theme changes in real-time

### Modern Components
- **Card-based Layout**: Clean, organized information display
- **Rounded Corners**: Modern, friendly appearance
- **Drop Shadows**: Subtle depth and hierarchy
- **Smooth Animations**: Polished user interactions
- **Responsive Design**: Adapts to different screen sizes

### Navigation
- **Sidebar Navigation**: Intuitive left-side navigation
- **Compact Mode**: Collapsible navigation for more screen space
- **Icon + Text**: Clear visual indicators
- **Status Integration**: Balance and sync status in navigation

## 🚀 Getting Started

### Using the Theme System

1. **Access Theme Settings**:
   - Go to `Settings` → `Theme` tab
   - Choose from Light, Dark, Auto, or Custom themes

2. **Custom Theme Creation**:
   - Select "Custom" from the theme dropdown
   - Click color buttons to customize:
     - Primary Background
     - Secondary Background  
     - Text Color
     - Accent Color
     - Border Color
   - Click "Apply Custom Theme" to save

3. **Font Customization**:
   - Enable "Use custom font"
   - Choose font family and size
   - Changes apply immediately

### Navigation

The modern navigation sidebar provides quick access to:
- 📊 **Overview**: Balance and recent transactions
- 📤 **Send**: Send DOGE to addresses
- 📥 **Receive**: Generate receiving addresses
- 📋 **History**: Transaction history
- 👥 **Address Book**: Manage saved addresses
- 💻 **Console**: Debug and RPC console
- ⚙️ **Settings**: Application configuration

### Overview Page

The redesigned overview page features:
- **Balance Cards**: Clear display of available, pending, and immature balances
- **Transaction List**: Recent transaction history
- **Network Status**: Connection and sync information
- **Quick Actions**: Send, Receive, History, and Refresh buttons

## 🛠️ Technical Implementation

### Core Components

#### ThemeManager
- Centralized theme management
- Color scheme definitions
- Font customization
- Style application

#### ModernCard
- Reusable card component
- Multiple card types (Default, Primary, Success, Warning, Error, Info)
- Shadow effects and animations
- Theme-aware styling

#### ModernNavigation
- Sidebar navigation component
- Compact/expanded modes
- Status integration
- Smooth transitions

#### ModernOverviewPage
- Card-based balance display
- Transaction list integration
- Network status monitoring
- Quick action buttons

### Integration Points

The modern UI system integrates with existing Dogecoin Core components:

1. **Options Dialog**: Theme switcher tab added
2. **Main Window**: Theme initialization
3. **Wallet Integration**: Balance and transaction display
4. **Client Integration**: Network status and sync information

## 🎯 Design Principles

### Accessibility
- High contrast ratios for readability
- Clear visual hierarchy
- Consistent spacing and typography
- Keyboard navigation support

### Performance
- Efficient theme switching
- Minimal resource usage
- Smooth animations
- Responsive interactions

### User Experience
- Intuitive navigation
- Clear visual feedback
- Consistent design language
- Customizable appearance

## 🔧 Customization

### Creating Custom Themes

1. **Color Selection**:
   ```cpp
   ThemeManager::ThemeColors customColors;
   customColors.primaryBackground = QColor(255, 255, 255);
   customColors.primaryText = QColor(33, 37, 41);
   customColors.primaryAccent = QColor(0, 123, 255);
   // ... set other colors
   ```

2. **Theme Application**:
   ```cpp
   ThemeManager* themeManager = ThemeManager::instance();
   themeManager->loadCustomTheme("MyTheme", customColors);
   themeManager->switchToCustom("MyTheme");
   ```

### Adding New Components

1. **Inherit from ModernCard**:
   ```cpp
   class MyCustomCard : public ModernCard {
       // Custom implementation
   };
   ```

2. **Apply Theme**:
   ```cpp
   ThemeManager::instance()->applyTheme(myWidget);
   ```

## 📱 Responsive Design

The modern UI adapts to different screen sizes:

- **Desktop**: Full sidebar navigation
- **Tablet**: Collapsible navigation
- **Mobile**: Compact mode with icons only

## 🎨 Color Schemes

### Light Theme
- Primary Background: #FFFFFF
- Secondary Background: #F8F9FA
- Primary Text: #212529
- Accent: #007BFF
- Success: #28A745
- Warning: #FFC107
- Error: #DC3545

### Dark Theme
- Primary Background: #121212
- Secondary Background: #212529
- Primary Text: #F8F9FA
- Accent: #007BFF
- Success: #28A745
- Warning: #FFC107
- Error: #DC3545

## 🚀 Future Enhancements

### Planned Features
- **More Animation**: Smooth page transitions
- **Advanced Customization**: More theme options
- **Accessibility**: Screen reader support
- **Mobile Optimization**: Touch-friendly interface

### Contributing
To contribute to the modern UI system:

1. Follow the existing code style
2. Test theme switching thoroughly
3. Ensure accessibility compliance
4. Update documentation

## 📄 License

This modern UI system is part of Dogecoin Core and is distributed under the MIT software license.

---

*Much modern, very theme, so sleek! 🐕*
