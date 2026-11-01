// SPDX-License-Identifier: MIT
#include "core/preferences.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QVBoxLayout>

int main(int argc,char **argv) {
    QApplication app(argc,argv); app.setApplicationName("Meteora Settings"); app.setApplicationVersion("0.2.0");
    Meteora::applyPalette(); Meteora::Preferences preferences;
    QWidget window; window.setWindowTitle("Meteora Settings"); window.setWindowIcon(QIcon(Meteora::assetPath("meteora.svg"))); window.resize(740,510); window.setMinimumSize(580,420);
    auto *root=new QHBoxLayout(&window); root->setContentsMargins(0,0,0,0); root->setSpacing(0);
    auto *sidebar=new QWidget; sidebar->setStyleSheet("background:#0e1721;"); sidebar->setFixedWidth(165); auto *side=new QVBoxLayout(sidebar); side->setContentsMargins(12,22,12,16);
    auto *logo=new QLabel; logo->setPixmap(QIcon(Meteora::assetPath("meteora.svg")).pixmap(40,40)); side->addWidget(logo);
    auto *brand=new QLabel("Meteora"); brand->setStyleSheet("font-size:18px;font-weight:600;padding:2px 0 12px;"); side->addWidget(brand);
    auto *nav=new QListWidget; nav->setAccessibleName("Settings pages");
    const QList<QStringList> pages={{"Appearance","preferences-desktop-wallpaper"},{"Panel","preferences-desktop-display"},{"Notifications","preferences-system-notifications"},{"About","help-about"}};
    for(const auto &p:pages) new QListWidgetItem(QIcon::fromTheme(p[1]),p[0],nav);
    nav->setCurrentRow(0); side->addWidget(nav,1); auto *version=new QLabel("0.2  /  DEMO"); version->setObjectName("eyebrow"); side->addWidget(version); root->addWidget(sidebar);
    auto *stack=new QStackedWidget; root->addWidget(stack,1);
    auto newPage=[&](const QString &title,const QString &description) {
        auto *page=new QWidget; auto *layout=new QVBoxLayout(page); layout->setContentsMargins(24,24,24,20); layout->setSpacing(15);
        auto *heading=new QLabel(title); heading->setObjectName("heading"); layout->addWidget(heading);
        auto *hint=new QLabel(description); hint->setWordWrap(true); hint->setObjectName("muted"); layout->addWidget(hint); stack->addWidget(page); return layout;
    };
    auto save=[&](const QString &key,const QVariant &value) { if(!preferences.setValue(key,value)) QMessageBox::warning(&window,"Could not save settings","Check that your configuration folder is writable."); };
    auto *appearance=newPage("Appearance","Charcoal surfaces. Ice-blue accents. Your workspace.");
    auto *thumbnail=new QLabel; thumbnail->setMinimumHeight(140); thumbnail->setMaximumHeight(190); thumbnail->setAlignment(Qt::AlignCenter); thumbnail->setStyleSheet("background:#0e1723;border:1px solid #34495c;border-radius:8px;"); appearance->addWidget(thumbnail,1);
    auto *filename=new QLabel; filename->setObjectName("muted"); filename->setTextFormat(Qt::PlainText); appearance->addWidget(filename);
    auto refresh=[&] {
        auto path=preferences.value("appearance/wallpaper").toString(); if(path.isEmpty() || !QFileInfo::exists(path)) path=Meteora::assetPath("meteora-wallpaper.png");
        filename->setText(QFileInfo(path).fileName());
        if(preferences.value("appearance/plain",false).toBool()) { thumbnail->clear(); return; }
        QImageReader reader(path); reader.setAllocationLimit(64); const QSize size=reader.size(); if(size.isValid()) reader.setScaledSize(size.scaled(QSize(420,185),Qt::KeepAspectRatio));
        thumbnail->setPixmap(QPixmap::fromImage(reader.read()).scaled(420,185,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    };
    auto *actions=new QHBoxLayout; auto *choose=new QPushButton("Choose wallpaper…"); auto *reset=new QPushButton("Restore default"); actions->addWidget(choose); actions->addWidget(reset); appearance->addLayout(actions);
    QObject::connect(choose,&QPushButton::clicked,&window,[&] {
        auto path=QFileDialog::getOpenFileName(&window,"Choose wallpaper",QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),"Images (*.png *.jpg *.jpeg *.webp *.bmp)");
        if(path.isEmpty()) return;
        QImageReader reader(path); if(!reader.canRead()) { QMessageBox::warning(&window,"Unsupported image","This file could not be read as an image."); return; }
        save("appearance/wallpaper",path); refresh();
    });
    QObject::connect(reset,&QPushButton::clicked,&window,[&] { save("appearance/wallpaper",QString()); refresh(); });
    auto *solid=new QCheckBox("Use a solid background to save memory"); solid->setChecked(preferences.value("appearance/plain",false).toBool()); appearance->addWidget(solid);
    QObject::connect(solid,&QCheckBox::toggled,&window,[&](bool b) { save("appearance/plain",b); refresh(); });
    auto *icons=new QCheckBox("Show Home, Documents and Downloads on desktop"); icons->setChecked(preferences.value("desktop/shortcuts",true).toBool()); appearance->addWidget(icons); QObject::connect(icons,&QCheckBox::toggled,&window,[&](bool b) { save("desktop/shortcuts",b); });
    auto *panel=newPage("Panel","A compact panel with your apps, windows and workspaces.");
    auto *floating=new QCheckBox("Floating panel"); floating->setChecked(preferences.value("panel/floating",true).toBool()); panel->addWidget(floating); QObject::connect(floating,&QCheckBox::toggled,&window,[&](bool b) { save("panel/floating",b); });
    auto *clock=new QCheckBox("Use a 24-hour clock"); clock->setChecked(preferences.value("panel/clock24",true).toBool()); panel->addWidget(clock); QObject::connect(clock,&QCheckBox::toggled,&window,[&](bool b) { save("panel/clock24",b); });
    auto *form=new QFormLayout; auto *height=new QSpinBox; height->setRange(44,64); height->setSuffix(" px"); height->setValue(preferences.value("panel/height",50).toInt()); height->setAccessibleName("Panel height"); form->addRow("Height",height); panel->addLayout(form); QObject::connect(height,&QSpinBox::valueChanged,&window,[&](int n) { save("panel/height",n); });
    auto *controls=new QLabel("Super  ·  Applications\nAlt + Tab  ·  Switch windows\nSuper + 1 / 2  ·  Workspaces\nSuper + arrows  ·  Arrange windows\nAlt + F4  ·  Close window\nAlt + drag  ·  Move window"); controls->setObjectName("muted"); controls->setStyleSheet("line-height:1.8;"); panel->addWidget(controls); panel->addStretch();
    auto *notices=newPage("Notifications","Stay informed without an always-running background animation.");
    auto *dnd=new QCheckBox("Do not disturb"); dnd->setChecked(preferences.value("notifications/dnd",false).toBool()); notices->addWidget(dnd); QObject::connect(dnd,&QCheckBox::toggled,&window,[&](bool b) { save("notifications/dnd",b); });
    auto *note=new QLabel("When enabled, popups stay hidden. Recent notifications remain in the panel's notification history.\n\nHistory is kept in memory, limited to 50 entries, and cleared when you log out."); note->setWordWrap(true); note->setObjectName("muted"); notices->addWidget(note); notices->addStretch();
    auto *about=newPage("Made to stay light.","Meteora Desktop 0.2 · Native C++ demo");
    auto *description=new QLabel("An original Qt Widgets shell with Openbox window management. No Plasma session, web runtime, compositor, blur or animation loop.\n\nBuilt for everyday machines, including older hardware. The desktop sleeps between events; the clock updates once a minute.\n\nThis demo uses X11. Wayland, a full system tray, lock screen and integrated network/audio settings are future work. Applications are supplied by you."); description->setWordWrap(true); about->addWidget(description); about->addStretch();
    auto *saved=new QLabel("Changes save automatically."); saved->setObjectName("muted"); about->addWidget(saved);
    QObject::connect(nav,&QListWidget::currentRowChanged,stack,&QStackedWidget::setCurrentIndex); refresh(); window.show(); return app.exec();
}
