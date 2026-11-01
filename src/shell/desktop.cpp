// SPDX-License-Identifier: MIT
#include "desktop.h"
#include <QApplication>
#include <QCalendarWidget>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMenu>
#include <QPushButton>
#include <QFrame>
#include <QLocale>
#include <QResizeEvent>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QScrollArea>
#include <QShortcut>
#include <QStandardPaths>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>
#include <QStyledItemDelegate>
#include <QTextLayout>

namespace Meteora {
namespace {
class ApplicationDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &,const QModelIndex &) const override { return {106,90}; }
    void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        p->save(); p->setRenderHint(QPainter::Antialiasing);
        const QRect r=option.rect.adjusted(3,2,-3,-2);
        if(option.state & (QStyle::State_Selected | QStyle::State_MouseOver)) {
            p->setPen(Qt::NoPen); p->setBrush(QColor(option.state & QStyle::State_Selected ? "#234560" : "#1d3347")); p->drawRoundedRect(r,7,7);
        }
        const auto icon=qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        icon.paint(p,QRect(r.center().x()-18,r.top()+5,36,36));
        p->setPen(QColor("#edf4fc")); p->setFont(option.font);
        QTextLayout text(index.data(Qt::DisplayRole).toString(),option.font);
        QTextOption textOption; textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); textOption.setAlignment(Qt::AlignHCenter); text.setTextOption(textOption);
        text.beginLayout(); qreal y=0;
        for(int n=0;n<2;++n) { auto line=text.createLine(); if(!line.isValid()) break; line.setLineWidth(r.width()-8); line.setPosition(QPointF(0,y)); y+=line.height(); }
        text.endLayout(); p->setClipRect(r); text.draw(p,QPointF(r.left()+4,r.top()+46)); p->restore();
    }
};
QToolButton *button(QWidget *parent,const QString &icon,const QString &name,const QString &fallback={}) {
    auto *b=new QToolButton(parent); b->setObjectName("panelButton");
    b->setIcon(QIcon::fromTheme(icon)); b->setIconSize(QSize(24,24));
    if (b->icon().isNull()) b->setText(fallback.isEmpty() ? name.left(1) : fallback);
    b->setToolTip(name); b->setAccessibleName(name); b->setCursor(Qt::PointingHandCursor);
    b->setFixedSize(38,36); return b;
}
void clearLayout(QLayout *layout) {
    while (auto *item=layout->takeAt(0)) { delete item->widget(); delete item; }
}
QJsonArray rectArray(const QRect &r) { return {r.x(),r.y(),r.width(),r.height()}; }
QStringList defaultFavorites() {
    return {"@settings","firefox.desktop","org.mozilla.firefox.desktop","chromium.desktop","thunar.desktop","org.xfce.Thunar.desktop","org.xfce.xfce4-terminal.desktop","xfce4-terminal.desktop","xterm.desktop"};
}
}
Wallpaper::Wallpaper(QWidget *parent) : QWidget(parent) { setAttribute(Qt::WA_OpaquePaintEvent); }
void Wallpaper::setSource(const QString &source,bool solid) {
    if (source==path && plain==solid && (!cached.isNull() || plain)) return;
    path=source; plain=solid; render();
}
void Wallpaper::render() {
    cached=QPixmap();
    if (!plain && !path.isEmpty() && !size().isEmpty()) {
        QImageReader reader(path); reader.setAutoTransform(true);
        const QSize imageSize=reader.size();
        if (imageSize.isValid()) {
            const QSize target=(size()*devicePixelRatioF());
            reader.setScaledSize(imageSize.scaled(target,Qt::KeepAspectRatioByExpanding));
            QImage decoded=reader.read();
            if (!decoded.isNull()) {
                decoded=decoded.scaled(target,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
                cached=QPixmap::fromImage(decoded.copy((decoded.width()-target.width())/2,(decoded.height()-target.height())/2,target.width(),target.height()));
                cached.setDevicePixelRatio(devicePixelRatioF());
            }
        }
    }
    update();
}
void Wallpaper::paintEvent(QPaintEvent *) { QPainter p(this); p.fillRect(rect(),QColor("#0e1723")); if (!cached.isNull()) p.drawPixmap(0,0,cached); }
void Wallpaper::resizeEvent(QResizeEvent *e) { QWidget::resizeEvent(e); render(); }
Desktop::Desktop(bool isPreview,QObject *parent) : QObject(parent),preview(isPreview),preferences(this),catalog(this),windows(this),notifications(this) {
    QImageReader::setAllocationLimit(64);
    qApp->installEventFilter(this);
    if (preview) {
        primary=new Wallpaper; primary->setWindowTitle("Meteora Desktop Preview"); primary->resize(1280,800); primary->setMinimumSize(640,480); backgrounds.append(primary);
    }
    buildPanel(); buildLauncher(); buildHistory();
    if (preview) {
        auto *hint=new QLabel("METEORA  /  PREVIEW",primary); hint->setObjectName("eyebrow"); hint->move(24,22); hint->adjustSize();
    } else {
        rebuildScreens();
        connect(qApp,&QGuiApplication::screenAdded,this,[this] { rebuildScreens(); });
        connect(qApp,&QGuiApplication::screenRemoved,this,[this] { rebuildScreens(); });
        connect(qApp,&QGuiApplication::primaryScreenChanged,this,[this] { rebuildScreens(); });
        connect(&windows,&WindowBridge::superPressed,this,&Desktop::toggleLauncher);
    }
    connect(&preferences,&Preferences::changed,this,&Desktop::applyPreferences);
    connect(&catalog,&AppCatalog::changed,this,&Desktop::populate);
    connect(&windows,&WindowBridge::windowsChanged,this,&Desktop::refreshTasks);
    connect(&windows,&WindowBridge::desktopChanged,this,[this] { refreshPager(); refreshTasks(); launcher->hide(); });
    connect(&notifications,&Notifications::historyChanged,this,&Desktop::refreshNotices);
    connect(&notifications,&Notifications::received,this,&Desktop::showNotice);
    connect(&notifications,&Notifications::NotificationClosed,this,[this](uint id,uint) { if(id==toastId) toast->hide(); });
    clockTimer.setSingleShot(true);
    const auto tick=[this] {
        const bool format24=preferences.value("panel/clock24",true).toBool();
        const auto now=QDateTime::currentDateTime();
        clock->setText(now.toString(format24 ? "HH:mm\nddd, MMM d" : "h:mm AP\nddd, MMM d"));
        clock->setToolTip(QLocale().toString(now,QLocale::LongFormat));
        clockTimer.start(60000-(now.time().second()*1000+now.time().msec()));
    };
    connect(&clockTimer,&QTimer::timeout,this,tick); tick();
    toastTimer.setSingleShot(true); connect(&toastTimer,&QTimer::timeout,toast,&QWidget::hide);
    applyPreferences(); refreshPager(); refreshTasks(); populate(); refreshNotices();
}
Desktop::~Desktop() {
    if (!preview) { delete panel; delete launcher; delete history; delete toast; }
    qDeleteAll(backgrounds);
}
void Desktop::buildPanel() {
    panel=new QWidget(preview ? primary : nullptr,preview ? Qt::Widget : Qt::Window | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    panel->setObjectName("panel"); panel->setWindowTitle("Meteora Panel");
    auto *bar=new QHBoxLayout(panel); bar->setContentsMargins(8,5,10,5); bar->setSpacing(5);
    menuButton=button(panel,"meteora","Applications (Super)"); menuButton->setIcon(QIcon(assetPath("meteora.svg"))); menuButton->setIconSize(QSize(30,30));
    connect(menuButton,&QToolButton::clicked,this,&Desktop::toggleLauncher); bar->addWidget(menuButton);
    auto *home=button(panel,"user-home","Home","⌂"); connect(home,&QToolButton::clicked,this,[this] { openFolder(QDir::homePath()); }); bar->addWidget(home);
    auto *terminal=button(panel,"utilities-terminal","Terminal",">_"); connect(terminal,&QToolButton::clicked,this,&Desktop::openTerminal); bar->addWidget(terminal);
    bar->addSpacing(7);
    auto *pages=new QWidget(panel); pager=new QHBoxLayout(pages); pager->setContentsMargins(0,0,0,0); pager->setSpacing(3); bar->addWidget(pages);
    auto *line=new QFrame(panel); line->setFrameShape(QFrame::VLine); line->setStyleSheet("color:#304355;"); bar->addWidget(line); bar->addSpacing(4);
    auto *scroll=new QScrollArea(panel); scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff); scroll->setStyleSheet("background:transparent;");
    auto *taskArea=new QWidget; taskArea->setStyleSheet("background:transparent;"); tasks=new QHBoxLayout(taskArea); tasks->setContentsMargins(0,0,0,0); tasks->setSpacing(4); tasks->addStretch(); scroll->setWidget(taskArea); bar->addWidget(scroll,1);
    auto *settings=button(panel,"preferences-system","Meteora Settings","⚙"); connect(settings,&QToolButton::clicked,this,&Desktop::openSettings); bar->addWidget(settings);
    bell=button(panel,"preferences-system-notifications","Notifications","N");
    connect(bell,&QToolButton::clicked,this,[this] { launcher->hide(); history->setVisible(!history->isVisible()); if(history->isVisible()) { refreshNotices(); layout(); history->raise(); } }); bar->addWidget(bell);
    clock=new QLabel(panel); clock->setAlignment(Qt::AlignCenter); clock->setMinimumWidth(84); clock->setStyleSheet("font-size:11px;color:#e6effa;"); bar->addWidget(clock);
    auto *show=button(panel,"user-desktop","Show desktop","—"); show->setFixedWidth(25);
    connect(show,&QToolButton::clicked,this,[this] { if(!preview) { showingDesktop=!showingDesktop; windows.showDesktop(showingDesktop); } }); bar->addWidget(show);
}
void Desktop::buildLauncher() {
    launcher=new QWidget(preview ? primary : nullptr,preview ? Qt::Widget : Qt::Popup | Qt::FramelessWindowHint);
    launcher->setObjectName("launcher"); launcher->setWindowTitle("Meteora Applications"); launcher->hide();
    auto *root=new QVBoxLayout(launcher); root->setContentsMargins(16,16,16,12); root->setSpacing(12);
    auto *header=new QHBoxLayout;
    auto *brand=new QLabel(launcher); brand->setPixmap(QIcon(assetPath("meteora.svg")).pixmap(32,32)); header->addWidget(brand);
    auto *title=new QLabel("Meteora",launcher); title->setStyleSheet("font-size:17px; font-weight:600;"); header->addWidget(title); header->addSpacing(18);
    search=new QLineEdit(launcher); search->setPlaceholderText("Search applications…"); search->setAccessibleName("Search applications"); search->setClearButtonEnabled(true); header->addWidget(search,1); root->addLayout(header);
    auto *body=new QHBoxLayout; body->setSpacing(12);
    categories=new QListWidget(launcher); categories->setAccessibleName("Application categories"); categories->setFixedWidth(166); categories->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff); categories->setStyleSheet("QListWidget::item { padding:7px 5px; }"); categories->setIconSize(QSize(18,18));
    const QList<QStringList> groups={
        {"Favorites","@favorites","emblem-favorite"},{"All Applications","","view-app-grid"},
        {"Development","Development","applications-development"},{"Graphics","Graphics","applications-graphics"},
        {"Internet","Network","applications-internet"},{"Multimedia","AudioVideo","applications-multimedia"},
        {"Office","Office","applications-office"},{"Settings","Settings","preferences-system"},
        {"System","System","applications-system"},{"Utilities","Utility","applications-utilities"}
    };
    for (const auto &g:groups) { auto *item=new QListWidgetItem(QIcon::fromTheme(g[2]),g[0],categories); item->setData(Qt::UserRole,g[1]); }
    categories->setCurrentRow(1); body->addWidget(categories);
    auto *right=new QVBoxLayout;
    apps=new QListWidget(launcher); apps->setAccessibleName("Installed applications"); apps->setViewMode(QListView::IconMode); apps->setResizeMode(QListView::Adjust); apps->setMovement(QListView::Static); apps->setIconSize(QSize(36,36)); apps->setGridSize(QSize(106,90)); apps->setSpacing(2); apps->setWordWrap(true); apps->setUniformItemSizes(true); apps->setItemDelegate(new ApplicationDelegate(apps)); apps->setSelectionMode(QAbstractItemView::SingleSelection); apps->setContextMenuPolicy(Qt::CustomContextMenu);
    right->addWidget(apps,1);
    empty=new QLabel("No applications in this category.",launcher); empty->setObjectName("muted"); empty->setWordWrap(true); right->addWidget(empty);
    body->addLayout(right,1); root->addLayout(body,1);
    auto *footer=new QHBoxLayout;
    auto *help=new QLabel("Enter to open · Right-click to pin",launcher); help->setObjectName("muted"); help->setStyleSheet("font-size:10px;"); footer->addWidget(help); footer->addStretch();
    auto *settings=new QPushButton("Settings",launcher); connect(settings,&QPushButton::clicked,this,[this] { launcher->hide(); openSettings(); }); footer->addWidget(settings);
    auto *leave=new QPushButton(preview ? "Close preview" : "Log out",launcher); connect(leave,&QPushButton::clicked,this,&Desktop::logout); footer->addWidget(leave); root->addLayout(footer);
    connect(search,&QLineEdit::textChanged,this,[this] { populate(); });
    connect(search,&QLineEdit::returnPressed,this,&Desktop::launchSelected);
    connect(categories,&QListWidget::currentItemChanged,this,[this] { populate(); });
    // Single click and keyboard Enter are distinct signals; avoid double launches.
    connect(apps,&QListWidget::itemClicked,this,[this] { launchSelected(); });
    connect(apps,&QListWidget::customContextMenuRequested,this,[this](QPoint p) {
        auto *item=apps->itemAt(p); if(!item) return;
        const QString id=item->data(Qt::UserRole).toString();
        const bool pinned=preferences.value("launcher/favorites",defaultFavorites()).toStringList().contains(id);
        QMenu menu; auto *pin=menu.addAction(pinned ? "Remove from Favorites" : "Pin to Favorites");
        if(menu.exec(apps->viewport()->mapToGlobal(p))==pin) toggleFavorite(id);
    });
}
void Desktop::buildHistory() {
    history=new QWidget(preview ? primary : nullptr,preview ? Qt::Widget : Qt::Popup | Qt::FramelessWindowHint); history->setObjectName("history"); history->setWindowTitle("Meteora Notifications"); history->hide();
    auto *root=new QVBoxLayout(history); root->setContentsMargins(16,16,16,12);
    auto *heading=new QLabel("Notifications",history); heading->setStyleSheet("font-size:18px;font-weight:600;"); root->addWidget(heading);
    noticeList=new QListWidget(history); noticeList->setWordWrap(true); root->addWidget(noticeList,1);
    auto *clear=new QPushButton("Clear history",history); connect(clear,&QPushButton::clicked,&notifications,&Notifications::clearHistory); root->addWidget(clear);
    toast=new QWidget(preview ? primary : nullptr,preview ? Qt::Widget : Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus); toast->setObjectName("notification"); toast->hide();
    auto *box=new QVBoxLayout(toast); box->setContentsMargins(16,12,16,12);
    toastTitle=new QLabel(toast); toastTitle->setTextFormat(Qt::PlainText); toastTitle->setWordWrap(true); toastTitle->setStyleSheet("color:#a9dcff;font-weight:600;"); box->addWidget(toastTitle);
    toastBody=new QLabel(toast); toastBody->setTextFormat(Qt::PlainText); toastBody->setWordWrap(true); box->addWidget(toastBody);
}
void Desktop::rebuildScreens() {
    if(preview) return;
    const auto screens=qApp->screens();
    for(auto *s:screens) disconnect(s,nullptr,this,nullptr);
    qDeleteAll(backgrounds); backgrounds.clear(); primary=nullptr; shortcuts=nullptr;
    primaryScreen=qApp->primaryScreen();
    for(auto *screen:screens) {
        auto *bg=new Wallpaper(nullptr); bg->setWindowTitle("Meteora Desktop");
        bg->setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        bg->setGeometry(screen->geometry()); windows.markDesktop(bg->winId()); backgrounds.append(bg);
        if(screen==primaryScreen) primary=bg;
        connect(screen,&QScreen::geometryChanged,this,[this] { rebuildScreens(); });
        bg->show(); bg->lower();
    }
    if(!primary) return;
    shortcuts=new QWidget(primary); auto *column=new QVBoxLayout(shortcuts); column->setContentsMargins(0,0,0,0); column->setSpacing(8);
    const QList<QStringList> folders={{"Home",QDir::homePath(),"user-home"},{"Documents",QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),"folder-documents"},{"Downloads",QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),"folder-download"}};
    for(const auto &folder:folders) {
        auto *b=button(shortcuts,folder[2],folder[0]); b->setObjectName("desktopIcon"); b->setText(folder[0]); b->setIconSize(QSize(32,32)); b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon); b->setFixedSize(88,70);
        connect(b,&QToolButton::clicked,this,[this,path=folder[1]] { openFolder(path); }); column->addWidget(b);
    }
    shortcuts->setGeometry(16,18,92,245); shortcuts->show();
    applyPreferences(); layout();
}
QRect Desktop::screenGeometry() const { return preview ? primary->rect() : (primaryScreen ? primaryScreen->geometry() : QRect(0,0,1024,768)); }
void Desktop::layout() {
    if(!primary) return;
    const QRect screen=screenGeometry();
    const int height=qBound(44,preferences.value("panel/height",50).toInt(),64);
    const int margin=preferences.value("panel/floating",true).toBool() ? 12 : 0;
    panel->setGeometry(screen.x()+margin,screen.bottom()-height-margin+1,screen.width()-2*margin,height);
    if(!preview) {
        int rootBottom=0; for(auto *s:qApp->screens()) rootBottom=qMax(rootBottom,s->geometry().bottom()+1);
        windows.markPanel(panel->winId(),panel->geometry(),rootBottom);
    }
    const int w=qMin(674,screen.width()-24), h=qMin(490,screen.height()-height-margin-32);
    launcher->resize(w,qMax(260,h)); launcher->move(screen.left()+12,panel->y()-launcher->height()-10);
    categories->setFixedWidth(w<600 ? 140 : 166);
    apps->setGridSize(w<600 ? QSize(94,86) : QSize(106,90));
    history->setGeometry(screen.right()-qMin(360,screen.width()-24)-11,panel->y()-qMin(420,screen.height()-height-40)-10,qMin(360,screen.width()-24),qMin(420,screen.height()-height-40));
    toast->setGeometry(screen.right()-qMin(350,screen.width()-24)-11,panel->y()-132,qMin(350,screen.width()-24),120);
}
void Desktop::applyPreferences() {
    QString path=preferences.value("appearance/wallpaper").toString();
    if(path.isEmpty() || !QFileInfo::exists(path)) path=assetPath("meteora-wallpaper.png");
    const bool plain=preferences.value("appearance/plain",false).toBool();
    for(auto *bg:backgrounds) bg->setSource(path,plain);
    if(shortcuts) shortcuts->setVisible(preferences.value("desktop/shortcuts",true).toBool());
    if(preferences.value("notifications/dnd",false).toBool()) toast->hide();
    layout(); populate();
    if(clock) {
        const auto now=QDateTime::currentDateTime();
        clock->setText(now.toString(preferences.value("panel/clock24",true).toBool() ? "HH:mm\nddd, MMM d" : "h:mm AP\nddd, MMM d"));
    }
}
void Desktop::show() {
    for(auto *bg:backgrounds) { bg->show(); if(!preview) bg->lower(); }
    layout(); panel->show(); panel->raise();
}
void Desktop::toggleLauncher() {
    if(launcher->isVisible()) { launcher->hide(); return; }
    history->hide(); search->clear(); populate(); layout(); launcher->show(); launcher->raise();
    if(!preview) windows.activatePopup(launcher->winId());
    launcher->activateWindow(); search->setFocus();
}
void Desktop::populate() {
    if(!apps || loadingApps) return;
    loadingApps=true;
    const QString selection=apps->currentItem() ? apps->currentItem()->data(Qt::UserRole).toString() : QString();
    apps->clear();
    const QString group=categories->currentItem() ? categories->currentItem()->data(Qt::UserRole).toString() : QString();
    const QString query=search->text();
    const auto favorites=preferences.value("launcher/favorites",defaultFavorites()).toStringList();
    const QString filter=query.isEmpty() && group!="@favorites" ? group : QString();
    auto matches=catalog.matching(query,filter);
    if((query.isEmpty() || QString("Meteora Settings preferences appearance wallpaper").contains(query,Qt::CaseInsensitive)) && (filter.isEmpty() || filter=="Settings"))
        matches.prepend({"@settings","Meteora Settings","Appearance, panel and notifications","Settings;","","preferences-system"});
    for(const auto &entry:matches) {
        if(query.isEmpty() && group=="@favorites" && !favorites.contains(entry.id)) continue;
        auto *item=new QListWidgetItem(AppCatalog::icon(entry),entry.name,apps); item->setData(Qt::UserRole,entry.id); item->setToolTip(entry.description.isEmpty() ? entry.name : entry.description);
        if(entry.id==selection) apps->setCurrentItem(item);
    }
    if(apps->count() && !apps->currentItem()) apps->setCurrentRow(0);
    empty->setVisible(apps->count()==0);
    empty->setText(!query.isEmpty() ? "No matching applications. Try another name or keyword." : group=="@favorites" ? "Pin an app from All Applications: right-click it and choose Pin to Favorites." : "No installed applications in this category.");
    loadingApps=false;
}
void Desktop::launchSelected() {
    auto *item=apps->currentItem(); if(!item) return;
    const QString id=item->data(Qt::UserRole).toString(); launcher->hide();
    if(id=="@settings") { openSettings(); return; }
    QString error; if(!catalog.launch(id,&error)) QMessageBox::warning(panel,"Could not open application",error);
}
void Desktop::toggleFavorite(const QString &id) {
    auto ids=preferences.value("launcher/favorites",defaultFavorites()).toStringList();
    if(ids.contains(id)) ids.removeAll(id); else ids.append(id);
    preferences.setValue("launcher/favorites",ids);
}
void Desktop::refreshTasks() {
    if(preview) return;
    const auto list=windows.windows(); QSet<unsigned long> live;
    for(const auto &win:list) {
        if(win.desktop!=-1 && win.desktop!=windows.currentDesktop()) continue;
        live.insert(win.id); auto *b=taskButtons.value(win.id,nullptr);
        if(!b) {
            b=button(panel,"application-x-executable",win.title); b->setCheckable(true); b->setFixedWidth(42); taskButtons.insert(win.id,b); tasks->insertWidget(tasks->count()-1,b);
            connect(b,&QToolButton::clicked,this,[this,id=win.id] { if(windows.activeWindow()==id) windows.minimize(id); else windows.activate(id); });
            b->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(b,&QToolButton::customContextMenuRequested,this,[this,b,id=win.id](QPoint p) {
                QMenu menu; auto *restore=menu.addAction("Activate"); auto *min=menu.addAction("Minimize");
                auto *desktops=menu.addMenu("Move to workspace");
                for(int n=0;n<windows.desktopCount();++n) { auto *a=desktops->addAction(QString::number(n+1)); connect(a,&QAction::triggered,this,[this,id,n] { windows.moveToDesktop(id,n); }); }
                menu.addSeparator(); auto *close=menu.addAction("Close window");
                auto *chosen=menu.exec(b->mapToGlobal(p)); if(chosen==restore) windows.activate(id); else if(chosen==min) windows.minimize(id); else if(chosen==close) windows.close(id);
            });
        }
        b->setToolTip(win.title); b->setAccessibleName(win.title); if(!win.icon.isNull()) b->setIcon(win.icon); b->setChecked(windows.activeWindow()==win.id);
    }
    const auto keys=taskButtons.keys(); for(auto id:keys) if(!live.contains(id)) { delete taskButtons.take(id); }
}
void Desktop::refreshPager() {
    clearLayout(pager);
    const int count=preview ? 2 : windows.desktopCount();
    for(int n=0;n<count;++n) {
        auto *b=new QToolButton(panel); b->setObjectName("panelButton"); b->setText(QString::number(n+1)); b->setFixedSize(26,28); b->setToolTip(QString("Workspace %1").arg(n+1)); b->setAccessibleName(b->toolTip()); b->setCheckable(true); b->setChecked(n==windows.currentDesktop());
        if(preview) b->setEnabled(false); else connect(b,&QToolButton::clicked,this,[this,n] { windows.setDesktop(n); }); pager->addWidget(b);
    }
}
void Desktop::refreshNotices() {
    noticeList->clear();
    for(const auto &n:notifications.history()) {
        auto *item=new QListWidgetItem(n.time.toString("HH:mm")+"   "+n.app+"\n"+n.summary+"\n"+n.body,noticeList); item->setSizeHint(QSize(290,100));
    }
    if(noticeList->count()==0) new QListWidgetItem(notifications.registered() ? "You're all caught up." : "Notifications are handled by your current desktop.",noticeList);
    bell->setToolTip(QString("Notifications (%1)").arg(notifications.history().size()));
}
void Desktop::showNotice(uint id) {
    if(preferences.value("notifications/dnd",false).toBool()) return;
    for(const auto &n:notifications.history()) if(n.id==id) {
        toastId=id; toastTitle->setText(n.summary); toastBody->setText(n.body.left(220)); layout(); toast->show(); toast->raise(); toastTimer.start(6000); break;
    }
}
void Desktop::openSettings() {
    const QString sibling=QCoreApplication::applicationDirPath()+"/meteora-settings";
    if(!QProcess::startDetached(QFile::exists(sibling) ? sibling : "meteora-settings",{})) QMessageBox::warning(panel,"Meteora Settings","Meteora Settings could not be started. Rebuild or reinstall Meteora.");
}
void Desktop::openFolder(const QString &path) {
    if(!QDir(path).exists()) { QMessageBox::information(panel,"Folder unavailable","This folder does not exist yet:\n"+path); return; }
    if(!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) QMessageBox::information(panel,"Open folder","No file manager is configured. Install or choose your preferred file manager.");
}
void Desktop::openTerminal() {
    for(const QString name: {"x-terminal-emulator","xfce4-terminal","qterminal","lxterminal","alacritty","kitty","konsole","xterm"}) {
        const QString program=QStandardPaths::findExecutable(name);
        if(!program.isEmpty() && QProcess::startDetached(program,{})) return;
    }
    QMessageBox::information(panel,"Terminal unavailable","Install your preferred terminal emulator to use this shortcut.");
}
void Desktop::logout() {
    launcher->hide();
    if(preview || QMessageBox::question(panel,"Log out of Meteora?","Save your work first. This ends your desktop session.",QMessageBox::Cancel | QMessageBox::Yes,QMessageBox::Cancel)==QMessageBox::Yes) qApp->quit();
}
bool Desktop::eventFilter(QObject *watched,QEvent *event) {
    if(event->type()==QEvent::KeyPress) {
        auto *key=static_cast<QKeyEvent *>(event);
        if(key->key()==Qt::Key_Escape) { launcher->hide(); history->hide(); return false; }
        if(watched==search && key->key()==Qt::Key_Down) { apps->setFocus(); if(apps->count() && !apps->currentItem()) apps->setCurrentRow(0); return true; }
        if(watched==apps && (key->key()==Qt::Key_Return || key->key()==Qt::Key_Enter)) { launchSelected(); return true; }
        if(watched==apps && key->key()==Qt::Key_Up && apps->currentRow()==0) { search->setFocus(); return true; }
    }
    if(preview && event->type()==QEvent::KeyRelease && static_cast<QKeyEvent *>(event)->key()==Qt::Key_Meta) { toggleLauncher(); return true; }
    if(event->type()==QEvent::MouseButtonPress) {
        auto *widget=qobject_cast<QWidget *>(watched);
        if(widget && backgrounds.contains(dynamic_cast<Wallpaper *>(widget))) { launcher->hide(); history->hide(); }
        if(watched==toast) toast->hide();
    }
    if(watched==primary && event->type()==QEvent::Resize && preview) layout();
    return QObject::eventFilter(watched,event);
}
QJsonObject Desktop::status() const {
    QJsonArray clients;
    for(const auto &w:windows.windows()) clients.append(QJsonObject{{"id",QString::number(w.id)},{"title",w.title},{"desktop",w.desktop},{"minimized",w.minimized}});
    QJsonArray results; for(int i=0;i<apps->count();++i) results.append(apps->item(i)->text());
    QJsonObject tasksJson; for(auto it=taskButtons.begin();it!=taskButtons.end();++it) tasksJson.insert(QString::number(it.key()),rectArray(QRect(it.value()->mapToGlobal(QPoint(0,0)),it.value()->size())));
    return {{"version","0.2.0"},{"preview",preview},{"launcherVisible",launcher->isVisible()},{"launcherRect",rectArray(launcher->geometry())},{"panelRect",rectArray(panel->geometry())},{"panelWindow",QString::number(panel->winId())},{"searchRect",rectArray(QRect(search->mapToGlobal(QPoint(0,0)),search->size()))},{"windows",clients},{"taskButtons",tasksJson},{"desktop",windows.currentDesktop()},{"desktopCount",windows.desktopCount()},{"applicationCount",catalog.entries().size()},{"visibleApplications",results},{"notifications",notifications.history().size()},{"notificationServer",notifications.registered()},{"toastVisible",toast->isVisible()},{"preferences",preferences.fileName()}};
}
bool Desktop::capture(const QString &path) {
    return preview ? primary->grab().save(path,"PNG") : (primaryScreen && primaryScreen->grabWindow(0).save(path,"PNG"));
}
}
