#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QSettings>
#include <QFileInfo>
#include <QStandardPaths>
#include <QGroupBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QDir>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Meteora Settings");
    QWidget window;
    window.setWindowTitle("Meteora Settings");
    window.resize(600, 420);
    window.setMinimumSize(440, 320);
    window.setStyleSheet(R"(
        QWidget { background:#121b28; color:#e9f3ff; font-family:'Noto Sans',sans-serif; font-size:13px; }
        QLabel#heading { font-size:23px; font-weight:700; color:#8acbff; }
        QLabel#hint { color:#9bb1c8; }
        QGroupBox { border:1px solid #3d5872; border-radius:13px; margin-top:16px; padding:15px; font-weight:600; }
        QGroupBox::title { subcontrol-origin:margin; left:15px; padding:0 6px; color:#8acbff; }
        QPushButton { background:#274a69; border:1px solid #3b7aa9; border-radius:9px; padding:10px 15px; font-weight:600; }
        QPushButton:hover { background:#35688f; }
        QCheckBox { spacing:10px; }
    )");
    auto *root = new QVBoxLayout(&window);
    root->setContentsMargins(28,24,28,24);
    root->setSpacing(18);
    auto *heading = new QLabel("◈  Meteora Settings");
    heading->setObjectName("heading");
    root->addWidget(heading);
    auto *subtitle = new QLabel("Personalize your desktop");
    subtitle->setObjectName("hint");
    root->addWidget(subtitle);
    auto *appearance = new QGroupBox("Appearance");
    auto *layout = new QVBoxLayout(appearance);
    layout->setSpacing(14);
    QSettings settings("Meteora", "Desktop");
    auto *wallpaper = new QLabel;
    wallpaper->setWordWrap(true);
    wallpaper->setObjectName("hint");
    auto refreshLabel = [&] {
        QString path = settings.value("appearance/wallpaper").toString();
        wallpaper->setText(path.isEmpty() ? "Wallpaper: Meteora default" : "Wallpaper: " + QFileInfo(path).fileName());
    };
    refreshLabel();
    layout->addWidget(wallpaper);
    auto *actions = new QHBoxLayout;
    auto *choose = new QPushButton("Choose wallpaper…");
    auto *reset = new QPushButton("Restore default");
    actions->addWidget(choose);
    actions->addWidget(reset);
    actions->addStretch();
    layout->addLayout(actions);
    QObject::connect(choose, &QPushButton::clicked, &window, [&] {
        QString start = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (start.isEmpty() || !QDir(start).exists()) start = QDir::homePath();
        QString path = QFileDialog::getOpenFileName(&window, "Choose wallpaper", start,
                                                     "Images (*.png *.jpg *.jpeg *.webp *.bmp)");
        if (path.isEmpty()) return;
        settings.setValue("appearance/wallpaper", path);
        settings.sync();
        refreshLabel();
    });
    QObject::connect(reset, &QPushButton::clicked, &window, [&] {
        settings.remove("appearance/wallpaper");
        settings.sync();
        refreshLabel();
    });
    root->addWidget(appearance);
    auto *about = new QGroupBox("About Meteora");
    auto *aboutLayout = new QVBoxLayout(about);
    auto *description = new QLabel("Meteora Desktop — independent C++ desktop project.\\nSettings are saved per user and the preview picks up wallpaper changes automatically.");
    description->setWordWrap(true);
    description->setObjectName("hint");
    aboutLayout->addWidget(description);
    root->addWidget(about);
    root->addStretch();
    window.show();
    return app.exec();
}
