#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSettings>
#include <QProcess>
#include <QDateTime>
#include <QTimer>
#include <QScreen>
#include <QKeyEvent>
#include <QMessageBox>
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QIcon>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QFont>
#include <QCursor>
#include <QFrame>
#include <QSizePolicy>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QShortcut>
#include <QApplication>
#include <QMouseEvent>
#include <QListView>
#include <algorithm>

struct AppEntry { QString name, exec, icon; };
static QList<AppEntry> discoverApps() {
    QList<AppEntry> apps;
    QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    dirs.removeDuplicates();
    for (const QString &dir : dirs) {
        QDir d(dir);
        for (const auto &file : d.entryList({"*.desktop"}, QDir::Files)) {
            QSettings s(d.filePath(file), QSettings::IniFormat);
            s.beginGroup("Desktop Entry");
            if (s.value("Type").toString() != "Application" || s.value("Hidden").toBool() || s.value("NoDisplay").toBool()) continue;
            const QString name = s.value("Name").toString();
            const QString exec = s.value("Exec").toString();
            if (!name.isEmpty() && !exec.isEmpty()) apps.append({name, exec, s.value("Icon").toString()});
        }
    }
    std::sort(apps.begin(), apps.end(), [](const AppEntry &a, const AppEntry &b){return a.name.toLower() < b.name.toLower();});
    return apps;
}

class Desktop : public QWidget {
    QPixmap wallpaper;
    QString defaultWallpaper;
    QString activeWallpaper;
    QWidget *launcher = nullptr;
    QListWidget *list = nullptr;
    QListWidget *categories = nullptr;
    QLineEdit *search = nullptr;
    QList<AppEntry> apps;
    bool metaHeld = false;
    bool metaUsedWithOtherKey = false;
    void toggleLauncher() {
        if (launcher->isVisible()) { launcher->hide(); return; }
        refresh();
        search->clear();
        launcher->show();
        launcher->raise();
        search->setFocus();
    }
    void activateSelected() {
        auto *item = list->currentItem();
        if (item) emit list->itemActivated(item);
    }
public:
    Desktop() {
        qApp->installEventFilter(this);
        setWindowTitle("Meteora Desktop");
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setCursor(Qt::ArrowCursor);
        setMinimumSize(800, 480);
        setFont(QFont("Noto Sans", 10));
        QString asset = QCoreApplication::applicationDirPath() + "/../share/meteora/meteora-wallpaper.png";
        if (!QFile::exists(asset)) asset = QCoreApplication::applicationDirPath() + "/assets/meteora-wallpaper.png";
        if (!QFile::exists(asset)) asset = QDir::currentPath() + "/assets/meteora-wallpaper.png";
        defaultWallpaper = asset;
        auto updateWallpaper = [this] {
            QSettings settings("Meteora", "Desktop");
            QString custom = settings.value("appearance/wallpaper").toString();
            QString desired = (!custom.isEmpty() && QFile::exists(custom)) ? custom : defaultWallpaper;
            if (desired != activeWallpaper) {
                activeWallpaper = desired;
                wallpaper.load(desired);
                update();
            }
        };
        updateWallpaper();
        auto *wallpaperTimer = new QTimer(this);
        connect(wallpaperTimer, &QTimer::timeout, this, updateWallpaper);
        wallpaperTimer->start(1500);
        setStyleSheet(R"(
            QWidget#panel { background:#1c2837; border:1px solid #536b82; border-radius:20px; }
            QWidget#launcher { background:#1a2635; border:1px solid #56728c; border-radius:22px; }
            QPushButton { color:#e8f2ff; background:#304358; border:1px solid #435d76; border-radius:12px; padding:9px 16px; font-weight:600; }
            QPushButton:hover { background:#315a80; border-color:#56a7ee; }
            QPushButton#menuButton { background:#214466; border:1px solid #3989c6; font-weight:700; }
            QLabel { color:#e8eef7; }
            QLabel#muted { color:#91a8bf; }
            QLabel#clock { font-weight:600; color:#dcecff; }
            QLineEdit { background:#202f41; color:#f0f7ff; border:1px solid #45698a; border-radius:12px; padding:11px; selection-background-color:#2d75b0; }
            QLineEdit:focus { border-color:#58b1f5; }
            QListWidget { background:transparent; color:#e8eef7; border:0; outline:0; }
            QListWidget::item { padding:11px; border-radius:9px; }
            QListWidget::item:hover { background:#233a51; }
            QListWidget::item:selected { background:#27557b; }
        )");
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(22,0,22,18);
        root->addStretch();
        auto *panel = new QWidget(this); panel->setObjectName("panel"); panel->setFixedHeight(60);
        auto *bar = new QHBoxLayout(panel); bar->setContentsMargins(16,6,16,6);
        auto *menu = new QPushButton("◈  Meteora", panel); menu->setObjectName("menuButton"); menu->setCursor(Qt::PointingHandCursor);
        connect(menu, &QPushButton::clicked, this, [this]{ toggleLauncher(); });
        bar->addWidget(menu); bar->addSpacing(14);
        auto *tagline = new QLabel("DESKTOP", panel); tagline->setObjectName("muted"); tagline->setStyleSheet("font-size:10px;letter-spacing:2px;"); bar->addWidget(tagline);
        
        bar->addStretch();
        auto *settingsButton = new QPushButton("⚙  Settings", panel);
        settingsButton->setCursor(Qt::PointingHandCursor);
        settingsButton->setToolTip("Open Meteora Settings");
        connect(settingsButton, &QPushButton::clicked, this, [this] {
            const QString sibling = QCoreApplication::applicationDirPath() + "/meteora-settings";
            const QString program = QFile::exists(sibling) ? sibling : "meteora-settings";
            if (!QProcess::startDetached(program, {}))
                QMessageBox::warning(this, "Meteora Settings", "Unable to start meteora-settings.");
        });
        bar->addWidget(settingsButton);
        bar->addSpacing(10);
        auto *closeSession = new QPushButton("Exit preview", panel);
        closeSession->setToolTip("Close the Meteora test session (not system shutdown)");
        closeSession->setCursor(Qt::PointingHandCursor);
        connect(closeSession, &QPushButton::clicked, this, []{ QApplication::quit(); });
        bar->addWidget(closeSession);
        bar->addSpacing(14);
        auto *clock = new QLabel(panel); clock->setObjectName("clock"); bar->addWidget(clock);
        auto tick = [clock]{clock->setText(QDateTime::currentDateTime().toString("ddd  dd MMM   HH:mm"));};
        auto *timer = new QTimer(this); connect(timer, &QTimer::timeout, this, tick); timer->start(10000); tick();
        root->addWidget(panel);
        launcher = new QWidget(this); launcher->setObjectName("launcher"); launcher->setFixedSize(680,500); launcher->hide();
        auto *layout = new QVBoxLayout(launcher);
        layout->setContentsMargins(18,18,18,14);
        auto *header = new QHBoxLayout;
        auto *title = new QLabel("◈  Meteora", launcher);
        title->setStyleSheet("font-size:18px;font-weight:700;color:#91cfff");
        header->addWidget(title);
        search = new QLineEdit(launcher);
        search->setPlaceholderText("Search applications...");
        search->setClearButtonEnabled(true);
        header->addWidget(search,1);
        layout->addLayout(header);
        auto *body = new QHBoxLayout;
        categories = new QListWidget(launcher);
        categories->setFixedWidth(170);
        categories->addItems({"Favorites","All Applications","Settings"});
        categories->setCurrentRow(0);
        body->addWidget(categories);
        list = new QListWidget(launcher);
        list->setViewMode(QListView::IconMode);
        list->setResizeMode(QListView::Adjust);
        list->setMovement(QListView::Static);
        list->setIconSize(QSize(42,42));
        list->setGridSize(QSize(112,100));
        list->setWordWrap(true);
        body->addWidget(list,1);
        layout->addLayout(body,1);
        auto *foot = new QLabel("Super: launcher   •   Enter: open   •   Esc: close",launcher);
        foot->setObjectName("muted");
        layout->addWidget(foot);
        connect(categories, &QListWidget::currentRowChanged, this, [this](int){ populate(); });
        connect(search, &QLineEdit::textChanged, this, [this]{ populate(); });
        connect(search, &QLineEdit::returnPressed, this, [this]{ activateSelected(); });
        auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
        escape->setContext(Qt::ApplicationShortcut);
        connect(escape, &QShortcut::activated, this, [this]{ if (launcher->isVisible()) launcher->hide(); });
        connect(list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item){
            int idx = item->data(Qt::UserRole).toInt();
            if (idx < 0 || idx >= apps.size()) return;
            // Desktop Exec syntax includes field codes; remove them before parsing.
            QString command = apps[idx].exec;
            command.replace(QRegularExpression("%[fFuUdDnNickvm]"), "");
            QStringList args = QProcess::splitCommand(command.trimmed());
            if (args.isEmpty()) return;
            QString program = args.takeFirst();
            if (program == "env") { // Do not try to reinterpret arbitrary env command syntax.
                QMessageBox::information(this,"Meteora Preview","This application's Exec uses env; launch it from your existing desktop for now."); return;
            }
            if (!QProcess::startDetached(program,args)) QMessageBox::warning(this,"Launch failed", "Could not start " + apps[idx].name);
            launcher->hide();
        });
        auto *panelShadow = new QGraphicsDropShadowEffect(panel);
        panelShadow->setBlurRadius(36); panelShadow->setOffset(0,4);
        panelShadow->setColor(QColor(0,0,0,140)); panel->setGraphicsEffect(panelShadow);
        auto *launcherShadow = new QGraphicsDropShadowEffect(launcher);
        launcherShadow->setBlurRadius(44); launcherShadow->setOffset(0,8);
        launcherShadow->setColor(QColor(0,0,0,170)); launcher->setGraphicsEffect(launcherShadow);
        refresh();
    }
    void refresh() { apps = discoverApps(); populate(); }
    void populate() {
        list->clear();
        int shown = 0;
        for (int i=0;i<apps.size();++i) {
            if (!apps[i].name.contains(search->text(), Qt::CaseInsensitive)) continue;
            if (search->text().isEmpty() && categories->currentRow() == 0 && shown >= 12) break;
            if (search->text().isEmpty() && categories->currentRow() == 2 && !apps[i].name.contains("settings", Qt::CaseInsensitive)) continue;
            auto *item = new QListWidgetItem(QIcon::fromTheme(apps[i].icon), apps[i].name, list);
            item->setData(Qt::UserRole,i);
            ++shown;
        }
        if (list->count()) list->setCurrentRow(0);
    }
protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            auto *key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_Meta) {
                if (!key->isAutoRepeat()) {
                    if (event->type() == QEvent::KeyPress) {
                        metaHeld = true;
                        metaUsedWithOtherKey = false;
                    } else {
                        const bool shouldToggle = metaHeld && !metaUsedWithOtherKey;
                        metaHeld = false;
                        if (shouldToggle) {
                            toggleLauncher();
                            return true;
                        }
                    }
                }
            } else if (event->type() == QEvent::KeyPress && metaHeld) {
                metaUsedWithOtherKey = true;
            }
        } else if (event->type() == QEvent::ApplicationDeactivate) {
            metaHeld = false;
        }
        return QWidget::eventFilter(watched, event);
    }
    void mousePressEvent(QMouseEvent *event) override {
        if (launcher->isVisible() && !launcher->geometry().contains(event->pos())) launcher->hide();
        QWidget::mousePressEvent(event);
    }
    void resizeEvent(QResizeEvent *e) override { QWidget::resizeEvent(e); launcher->move(22,height()-60-launcher->height()-30); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#10151c"));
        if (!wallpaper.isNull()) {
            QSize scaled = wallpaper.size(); scaled.scale(size(), Qt::KeepAspectRatioByExpanding);
            QRect dst((width()-scaled.width())/2,(height()-scaled.height())/2,scaled.width(),scaled.height());
            p.drawPixmap(dst, wallpaper);
            p.fillRect(rect(), QColor(0,0,0,24));
        }
    }
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Escape && launcher->isVisible()) launcher->hide();
        else QWidget::keyPressEvent(event);
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Meteora Desktop Preview");
    Desktop desktop;
    // Maximized frameless avoids some virtualized Wayland fullscreen pointer issues.
    desktop.showMaximized();
    return app.exec();
}
