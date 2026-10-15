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
        setFont(QFont("Sans Serif", 10));
        QString asset = QCoreApplication::applicationDirPath() + "/../share/meteora/meteora-wallpaper.png";
        if (!QFile::exists(asset)) asset = QCoreApplication::applicationDirPath() + "/assets/meteora-wallpaper.png";
        if (!QFile::exists(asset)) asset = QDir::currentPath() + "/assets/meteora-wallpaper.png";
        wallpaper.load(asset);
        setStyleSheet("QWidget#panel {background:#151e2b; border:1px solid #344b63; border-radius:16px;}"
                      "QWidget#launcher {background:#151e2b; border:1px solid #3b5d7c; border-radius:16px;}"
                      "QPushButton {color:#e8eef7;background:#253447;border:0;border-radius:8px;padding:9px 14px;}"
                      "QPushButton:hover {background:#315579;}"
                      "QLabel {color:#e8eef7;}"
                      "QLineEdit {background:#253447;color:white;border:1px solid #379bff;border-radius:7px;padding:8px;}"
                      "QListWidget {background:#171f2a;color:#e8eef7;border:0;outline:0;}"
                      "QListWidget::item {padding:8px;} QListWidget::item:selected {background:#24527c;border-radius:6px;}");
        auto *root = new QVBoxLayout(this);
        root->setContentsMargins(18,0,18,14);
        root->addStretch();
        auto *panel = new QWidget(this); panel->setObjectName("panel"); panel->setFixedHeight(56);
        auto *bar = new QHBoxLayout(panel); bar->setContentsMargins(14,5,14,5);
        auto *menu = new QPushButton("◈  Meteora", panel);
        connect(menu, &QPushButton::clicked, this, [this]{ launcher->setVisible(!launcher->isVisible()); if (launcher->isVisible()) { refresh(); search->setFocus(); } });
        bar->addWidget(menu); bar->addSpacing(12);
        
        bar->addStretch();
        auto *clock = new QLabel(panel); bar->addWidget(clock);
        auto tick = [clock]{clock->setText(QDateTime::currentDateTime().toString("ddd  dd MMM   HH:mm"));};
        auto *timer = new QTimer(this); connect(timer, &QTimer::timeout, this, tick); timer->start(10000); tick();
        root->addWidget(panel);
        launcher = new QWidget(this); launcher->setObjectName("launcher"); launcher->setFixedSize(390,420); launcher->hide();
        auto *layout = new QVBoxLayout(launcher); layout->setContentsMargins(14,14,14,14);
        auto *title = new QLabel("Applications", launcher); title->setStyleSheet("font-size:16px;font-weight:700;color:#7bc2ff"); layout->addWidget(title);
        search = new QLineEdit(launcher); search->setPlaceholderText("Search installed applications..."); layout->addWidget(search);
        list = new QListWidget(launcher); layout->addWidget(list);
        auto *foot = new QLabel("Installed applications", launcher); foot->setStyleSheet("color:#8da2b9;font-size:11px"); layout->addWidget(foot);
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
            p.fillRect(rect(), QColor(0,0,0,45));
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
