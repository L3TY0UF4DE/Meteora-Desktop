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
    QWidget *launcher = nullptr;
    QListWidget *list = nullptr;
    QLineEdit *search = nullptr;
    QList<AppEntry> apps;
public:
    Desktop() {
        setWindowTitle("Meteora Desktop");
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setCursor(Qt::ArrowCursor);
        setMinimumSize(800, 480);
        setFont(QFont("Noto Sans", 10));
        QString asset = QCoreApplication::applicationDirPath() + "/../share/meteora/meteora-wallpaper.png";
        if (!QFile::exists(asset)) asset = QCoreApplication::applicationDirPath() + "/assets/meteora-wallpaper.png";
        if (!QFile::exists(asset)) asset = QDir::currentPath() + "/assets/meteora-wallpaper.png";
        wallpaper.load(asset);
        setStyleSheet(R"(
            QWidget#panel { background:#141d29; border:1px solid #35516c; border-radius:17px; }
            QWidget#launcher { background:#121b28; border:1px solid #416487; border-radius:18px; }
            QPushButton { color:#e8f2ff; background:#25384d; border:1px solid #354e68; border-radius:10px; padding:9px 16px; font-weight:600; }
            QPushButton:hover { background:#315a80; border-color:#56a7ee; }
            QPushButton#menuButton { background:#214466; border:1px solid #3989c6; font-weight:700; }
            QLabel { color:#e8eef7; }
            QLabel#muted { color:#91a8bf; }
            QLabel#clock { font-weight:600; color:#dcecff; }
            QLineEdit { background:#202f41; color:#f0f7ff; border:1px solid #45698a; border-radius:10px; padding:11px; selection-background-color:#2d75b0; }
            QLineEdit:focus { border-color:#58b1f5; }
            QListWidget { background:transparent; color:#e8eef7; border:0; outline:0; }
            QListWidget::item { padding:11px; border-radius:9px; }
            QListWidget::item:hover { background:#233a51; }
            QListWidget::item:selected { background:#27557b; }
        )");
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(18,0,18,14);
        root->addStretch();
        auto *panel = new QWidget(this); panel->setObjectName("panel"); panel->setFixedHeight(56);
        auto *bar = new QHBoxLayout(panel); bar->setContentsMargins(14,5,14,5);
        auto *menu = new QPushButton("◈  Meteora", panel); menu->setObjectName("menuButton"); menu->setCursor(Qt::PointingHandCursor);
        connect(menu, &QPushButton::clicked, this, [this]{ launcher->setVisible(!launcher->isVisible()); if (launcher->isVisible()) { refresh(); search->setFocus(); } });
        bar->addWidget(menu); bar->addSpacing(14);
        auto *tagline = new QLabel("DESKTOP", panel); tagline->setObjectName("muted"); tagline->setStyleSheet("font-size:10px;letter-spacing:2px;"); bar->addWidget(tagline);
        
        bar->addStretch();
        auto *clock = new QLabel(panel); clock->setObjectName("clock"); bar->addWidget(clock);
        auto tick = [clock]{clock->setText(QDateTime::currentDateTime().toString("ddd  dd MMM   HH:mm"));};
        auto *timer = new QTimer(this); connect(timer, &QTimer::timeout, this, tick); timer->start(10000); tick();
        root->addWidget(panel);
        launcher = new QWidget(this); launcher->setObjectName("launcher"); launcher->setFixedSize(410,460); launcher->hide();
        auto *layout = new QVBoxLayout(launcher); layout->setContentsMargins(20,20,20,18); layout->setSpacing(12);
        auto *title = new QLabel("METEORA   /   Applications", launcher); title->setStyleSheet("font-size:16px;font-weight:700;color:#82c8ff"); layout->addWidget(title);
        search = new QLineEdit(launcher); search->setPlaceholderText("Search applications..."); search->setClearButtonEnabled(true); layout->addWidget(search);
        list = new QListWidget(launcher); list->setIconSize(QSize(24,24)); list->setSpacing(3); layout->addWidget(list);
        auto *foot = new QLabel("Your installed applications  •  Esc to close", launcher); foot->setObjectName("muted"); foot->setStyleSheet("font-size:11px"); layout->addWidget(foot);
        connect(search, &QLineEdit::textChanged, this, [this]{ populate(); });
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
        panelShadow->setBlurRadius(28); panelShadow->setOffset(0,4);
        panelShadow->setColor(QColor(0,0,0,140)); panel->setGraphicsEffect(panelShadow);
        auto *launcherShadow = new QGraphicsDropShadowEffect(launcher);
        launcherShadow->setBlurRadius(32); launcherShadow->setOffset(0,8);
        launcherShadow->setColor(QColor(0,0,0,170)); launcher->setGraphicsEffect(launcherShadow);
        refresh();
    }
    void refresh() { apps = discoverApps(); populate(); }
    void populate() {
        list->clear();
        for (int i=0;i<apps.size();++i) {
            if (!apps[i].name.contains(search->text(), Qt::CaseInsensitive)) continue;
            auto *item = new QListWidgetItem(QIcon::fromTheme(apps[i].icon), apps[i].name, list);
            item->setData(Qt::UserRole,i);
        }
    }
protected:
    void resizeEvent(QResizeEvent *e) override { QWidget::resizeEvent(e); launcher->move(18,height()-56-launcher->height()-24); }
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
