// SPDX-License-Identifier: MIT
#include "preferences.h"
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPalette>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QIcon>
#include <QFont>

namespace Meteora {
QString assetPath(const QString &name) {
    const QStringList roots = {
        QCoreApplication::applicationDirPath() + "/../share/meteora",
        QCoreApplication::applicationDirPath() + "/assets",
        QStringLiteral(METEORA_ASSET_DIR)
    };
    for (const auto &root : roots)
        if (QFile::exists(root + '/' + name)) return root + '/' + name;
    return {};
}
void applyPalette() {
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window, QColor("#131b24"));
    p.setColor(QPalette::WindowText, QColor("#e7eef7"));
    p.setColor(QPalette::Base, QColor("#0f161f"));
    p.setColor(QPalette::AlternateBase, QColor("#1c2835"));
    p.setColor(QPalette::Text, QColor("#e7eef7"));
    p.setColor(QPalette::Button, QColor("#1d2c3b"));
    p.setColor(QPalette::ButtonText, QColor("#e7eef7"));
    p.setColor(QPalette::Highlight, QColor("#235780"));
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, QColor("#1b2938"));
    p.setColor(QPalette::ToolTipText, QColor("#eef5ff"));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor("#718399"));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#718399"));
    QApplication::setPalette(p);
    auto f = QApplication::font(); f.setPointSize(10); QApplication::setFont(f);
    QIcon::setFallbackThemeName("Adwaita");
    QIcon::setThemeName("Meteora");
    qApp->setStyleSheet(styleSheet());
}
QString styleSheet() {
    return QStringLiteral(R"(
        QToolTip { color:#e7eef7; background:#1b2938; border:1px solid #3a5168; padding:6px; }
        QWidget#panel, QWidget#launcher, QWidget#notification, QWidget#history {
            background:#111b26; border:1px solid #3a4d60; border-radius:10px;
        }
        QLabel { background:transparent; }
        QLabel#muted { color:#91a3b8; }
        QLabel#heading { color:#eef6ff; font-size:23px; font-weight:600; }
        QLabel#eyebrow { color:#79bbef; font-size:10px; font-weight:600; }
        QPushButton, QToolButton { color:#e7eef7; background:#1b2b3b; border:1px solid #2e4358; border-radius:6px; padding:7px 10px; }
        QPushButton:hover, QToolButton:hover { background:#253f56; border-color:#5489b2; }
        QPushButton:pressed, QToolButton:pressed, QToolButton:checked { background:#244f70; border-color:#64b8f4; }
        QPushButton:focus, QToolButton:focus { border-color:#64b8f4; }
        QToolButton#panelButton { background:transparent; border:1px solid transparent; padding:4px; }
        QToolButton#panelButton:hover { background:#22394e; border-color:#3e627d; }
        QToolButton#panelButton:checked { background:#233e55; border-bottom:2px solid #65bbfa; }
        QToolButton#desktopIcon { background:transparent; border:0; padding:6px; color:#f1f7ff; }
        QToolButton#desktopIcon:hover { background:#203e58; border-radius:7px; }
        QLineEdit, QComboBox { background:#0e1721; border:1px solid #33495d; border-radius:6px; padding:8px; color:#edf5ff; }
        QLineEdit:focus { border-color:#68baff; }
        QListWidget { background:transparent; color:#e7eef7; border:0; outline:0; }
        QListWidget::item { padding:9px; border-radius:6px; }
        QListWidget::item:selected { background:#234560; color:#f4faff; }
        QListWidget::item:hover { background:#1d3347; }
        QGroupBox { border:1px solid #304254; border-radius:8px; margin-top:15px; padding:18px 14px 12px; }
        QGroupBox::title { subcontrol-origin:margin; left:14px; padding:0 5px; color:#9dd5ff; }
        QCheckBox { spacing:9px; }
        QScrollBar:vertical { background:transparent; width:10px; margin:0; }
        QScrollBar::handle:vertical { background:#3b5267; border-radius:4px; min-height:25px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QMenu { background:#152332; border:1px solid #3c5369; padding:5px; }
        QMenu::item { padding:8px 24px 8px 12px; }
        QMenu::item:selected { background:#264b6a; }
    )");
}
Preferences::Preferences(QObject *parent) : QObject(parent) {
    QDir().mkpath(QFileInfo(fileName()).absolutePath());
    debounce.setSingleShot(true); debounce.setInterval(80);
    connect(&debounce, &QTimer::timeout, this, [this] { watch(); emit changed(); });
    connect(&watcher, &QFileSystemWatcher::fileChanged, this, [this] { debounce.start(); });
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [this] { debounce.start(); });
    watch();
}
QString Preferences::fileName() const { return QSettings("Meteora", "Desktop").fileName(); }
QVariant Preferences::value(const QString &key, const QVariant &fallback) const {
    QSettings s("Meteora", "Desktop"); s.sync(); return s.value(key, fallback);
}
bool Preferences::setValue(const QString &key, const QVariant &value) {
    QSettings s("Meteora", "Desktop"); s.setValue(key, value); s.sync();
    watch(); emit changed(); return s.status() == QSettings::NoError;
}
void Preferences::watch() {
    const QString dir = QFileInfo(fileName()).absolutePath();
    if (!watcher.directories().contains(dir)) watcher.addPath(dir);
    if (QFile::exists(fileName()) && !watcher.files().contains(fileName())) watcher.addPath(fileName());
}
}
