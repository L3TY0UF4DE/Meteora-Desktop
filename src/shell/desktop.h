// SPDX-License-Identifier: MIT
#pragma once
#include "core/preferences.h"
#include "core/appcatalog.h"
#include "core/windowbridge.h"
#include "notifications.h"
#include <QWidget>
#include <QPixmap>
#include <QJsonObject>
#include <QToolButton>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QPointer>
#include <QScreen>

namespace Meteora {
class Wallpaper : public QWidget {
public:
    explicit Wallpaper(QWidget *parent=nullptr);
    void setSource(const QString &path, bool plain);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
private:
    QString path;
    bool plain=false;
    QPixmap cached;
    void render();
};
class Desktop : public QObject {
    Q_OBJECT
public:
    explicit Desktop(bool preview, QObject *parent=nullptr);
    ~Desktop() override;
    void show();
    void toggleLauncher();
    void openSettings();
    QJsonObject status() const;
    bool capture(const QString &path);
protected:
    bool eventFilter(QObject *,QEvent *) override;
private:
    bool preview;
    Preferences preferences;
    AppCatalog catalog;
    WindowBridge windows;
    Notifications notifications;
    QList<Wallpaper *> backgrounds;
    Wallpaper *primary = nullptr;
    QWidget *panel = nullptr, *launcher = nullptr, *history = nullptr, *toast = nullptr;
    QWidget *shortcuts = nullptr;
    QHBoxLayout *tasks = nullptr, *pager = nullptr;
    QHash<unsigned long,QToolButton *> taskButtons;
    QListWidget *categories = nullptr, *apps = nullptr, *noticeList = nullptr;
    QLineEdit *search = nullptr;
    QLabel *empty = nullptr, *clock = nullptr, *toastTitle = nullptr, *toastBody = nullptr;
    QToolButton *bell = nullptr, *menuButton = nullptr;
    QTimer clockTimer, toastTimer;
    QPointer<QScreen> primaryScreen;
    bool showingDesktop = false;
    bool loadingApps = false;
    uint toastId=0;
    void buildPanel();
    void buildLauncher();
    void buildHistory();
    void rebuildScreens();
    void layout();
    void applyPreferences();
    void populate();
    void launchSelected();
    void toggleFavorite(const QString &id);
    void refreshTasks();
    void refreshPager();
    void refreshNotices();
    void showNotice(uint id);
    void openFolder(const QString &path);
    void openTerminal();
    void logout();
    QRect screenGeometry() const;
};
}
