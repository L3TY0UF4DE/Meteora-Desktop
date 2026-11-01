// SPDX-License-Identifier: MIT
// GIO owns Desktop Entry parsing/launching: quoting, % fields, env, localization,
// Hidden overrides, TryExec, Terminal and D-Bus activation. No command shell.
#include <gio/gdesktopappinfo.h>
#include "appcatalog.h"
#include <QApplication>
#include <QDirIterator>
#include <QStandardPaths>
#include <QStyle>
#include <algorithm>

namespace Meteora {
AppCatalog::AppCatalog(QObject *parent) : QObject(parent) {
    debounce.setSingleShot(true); debounce.setInterval(200);
    connect(&debounce, &QTimer::timeout, this, &AppCatalog::reload);
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [this] { debounce.start(); });
    connect(&watcher, &QFileSystemWatcher::fileChanged, this, [this] { debounce.start(); });
    reload();
}
void AppCatalog::watchDirectories() {
    if (!watcher.directories().isEmpty()) watcher.removePaths(watcher.directories());
    if (!watcher.files().isEmpty()) watcher.removePaths(watcher.files());
    QStringList paths;
    for (const auto &root : QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
        if (!QDir(root).exists()) { if (QDir(QFileInfo(root).absolutePath()).exists()) paths << QFileInfo(root).absolutePath(); continue; }
        paths << root;
        QDirIterator it(root, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString p = it.next();
            if (it.fileInfo().isDir() || p.endsWith(".desktop")) paths << p;
        }
    }
    paths.removeDuplicates(); if (!paths.isEmpty()) watcher.addPaths(paths);
}
void AppCatalog::reload() {
    apps.clear();
    GList *all = g_app_info_get_all();
    for (GList *node = all; node; node = node->next) {
        auto *info = G_APP_INFO(node->data);
        if (!G_IS_DESKTOP_APP_INFO(info) || !g_app_info_should_show(info)) continue;
        auto *desktop = G_DESKTOP_APP_INFO(info);
        if (g_desktop_app_info_get_is_hidden(desktop)) continue;
        const char *id = g_app_info_get_id(info);
        if (!id) continue;
        AppEntry entry;
        entry.id = QString::fromUtf8(id);
        entry.name = QString::fromUtf8(g_app_info_get_display_name(info));
        entry.description = QString::fromUtf8(g_app_info_get_description(info));
        entry.categories = QString::fromUtf8(g_desktop_app_info_get_categories(desktop));
        auto words = g_desktop_app_info_get_keywords(desktop);
        if (words) for (; *words; ++words) entry.keywords += QString::fromUtf8(*words) + ' ';
        char *icon = g_desktop_app_info_get_string(desktop, "Icon");
        if (icon) entry.iconName = QString::fromUtf8(icon);
        g_free(icon);
        apps.append(entry);
    }
    g_list_free_full(all, g_object_unref);
    std::sort(apps.begin(), apps.end(), [](const auto &a, const auto &b) { return QString::localeAwareCompare(a.name,b.name) < 0; });
    watchDirectories(); emit changed();
}
QList<AppEntry> AppCatalog::matching(const QString &query, const QString &category) const {
    QList<AppEntry> result;
    const auto words = query.simplified().split(' ', Qt::SkipEmptyParts);
    for (const auto &entry : apps) {
        if (!category.isEmpty() && !entry.categories.split(';').contains(category)) continue;
        const QString hay = entry.name + ' ' + entry.description + ' ' + entry.keywords + ' ' + entry.id;
        bool match = true;
        for (const auto &word : words) if (!hay.contains(word, Qt::CaseInsensitive)) { match = false; break; }
        if (match) result.append(entry);
    }
    if (!query.isEmpty()) std::stable_sort(result.begin(), result.end(), [&](const auto &a, const auto &b) {
        return a.name.startsWith(query, Qt::CaseInsensitive) && !b.name.startsWith(query, Qt::CaseInsensitive);
    });
    return result;
}
bool AppCatalog::launch(const QString &id, QString *error) const {
    auto *info = g_desktop_app_info_new(id.toUtf8().constData());
    if (!info) { if (error) *error = "The application is no longer installed."; return false; }
    GError *failure = nullptr;
    auto *context = g_app_launch_context_new();
    const bool ok = g_app_info_launch(G_APP_INFO(info), nullptr, context, &failure);
    if (!ok && error) *error = failure ? QString::fromUtf8(failure->message) : "Application could not be started.";
    g_clear_error(&failure); g_object_unref(context); g_object_unref(info); return ok;
}
QIcon AppCatalog::icon(const AppEntry &entry) {
    QIcon icon = entry.iconName.startsWith('/') ? QIcon(entry.iconName) : QIcon::fromTheme(entry.iconName);
    if (icon.isNull()) icon = QIcon::fromTheme("application-x-executable", QApplication::style()->standardIcon(QStyle::SP_ComputerIcon));
    return icon;
}
}
