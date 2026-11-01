// SPDX-License-Identifier: MIT
#include "core/appcatalog.h"
#include "core/preferences.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

static QString tempRoot;
static void write(const QString &path,const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile f(path); if(!f.open(QIODevice::WriteOnly)) qFatal("Cannot create fixture"); f.write(data); f.close();
}
static QByteArray entry(const QByteArray &name,const QByteArray &extra={}) {
    return "[Desktop Entry]\nType=Application\nName="+name+"\nExec=/bin/true\nCategories=Utility;\n"+extra;
}
class CoreTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        const QString user=tempRoot+"/user/applications/",system=tempRoot+"/system/applications/";
        write(user+"visible.desktop",entry("Visible app","Keywords=testing;needle;\n"));
        write(system+"override.desktop",entry("System duplicate"));
        write(user+"override.desktop",entry("User override"));
        write(system+"masked.desktop",entry("Should be hidden"));
        write(user+"masked.desktop","[Desktop Entry]\nHidden=true\n");
        write(user+"hidden.desktop",entry("Hidden app","NoDisplay=true\n"));
        write(user+"other.desktop",entry("Other desktop","OnlyShowIn=GNOME;\n"));
        write(user+"meteora.desktop",entry("Meteora only","OnlyShowIn=Meteora;\n"));
        write(user+"missing.desktop",entry("Missing executable","TryExec=/not/a/real/program\n"));
        write(user+"nested/nested.desktop",entry("Nested app"));
        // Use an executable fixture so Exec argument handling is observed directly.
        const QString script=tempRoot+"/capture-args";
        write(script,"#!/bin/sh\nprintf '%s\\n' \"$METEORA_TEST\" \"$@\" > \""+(tempRoot+"/launch-result").toUtf8()+"\"\n");
        QFile::setPermissions(script,QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        write(user+"launch.desktop","[Desktop Entry]\nType=Application\nName=Quoted launch\nIcon=application-x-executable\nExec=env METEORA_TEST=passed \""+script.toUtf8()+"\" \"two words\" %% %c %k %f\n");
    }
    void discoveryHonorsDesktopSpec() {
        Meteora::AppCatalog apps;
        auto names=[&] { QStringList n; for(const auto &e:apps.entries()) n<<e.name; return n; }();
        QVERIFY(names.contains("Visible app")); QVERIFY(names.contains("Nested app"));
        QVERIFY(names.contains("User override")); QVERIFY(!names.contains("System duplicate"));
        QVERIFY(!names.contains("Should be hidden")); QVERIFY(!names.contains("Hidden app"));
        QVERIFY(!names.contains("Missing executable")); QVERIFY(!names.contains("Other desktop"));
        QVERIFY(names.contains("Meteora only"));
        QCOMPARE(apps.matching("needle").size(),1); QCOMPARE(apps.matching("visible needle").size(),1);
        QVERIFY(apps.matching("absent").isEmpty()); QVERIFY(apps.matching("","Graphics").isEmpty());
    }
    void desktopExecIsLaunchedCorrectly() {
        Meteora::AppCatalog apps; QString error;
        QVERIFY2(apps.launch("launch.desktop",&error),qPrintable(error));
        QTRY_VERIFY(QFile::exists(tempRoot+"/launch-result"));
        QFile output(tempRoot+"/launch-result"); QVERIFY(output.open(QIODevice::ReadOnly));
        const QString result=QString::fromUtf8(output.readAll());
        QVERIFY(result.startsWith("passed\ntwo words\n%\nQuoted launch\n"));
        QVERIFY(result.contains("launch.desktop\n")); QVERIFY(!result.contains("%f"));
        QVERIFY(!apps.launch("nonexistent.desktop",&error)); QVERIFY(!error.isEmpty());
    }
    void noticesApplicationChangesWithoutPolling() {
        Meteora::AppCatalog apps; QSignalSpy signal(&apps,&Meteora::AppCatalog::changed);
        write(tempRoot+"/user/applications/added.desktop",entry("Added live"));
        QTRY_VERIFY_WITH_TIMEOUT(!apps.matching("Added live").isEmpty(),5000);
        QVERIFY(signal.count()>0);
        QFile::remove(tempRoot+"/user/applications/added.desktop");
        QTRY_VERIFY_WITH_TIMEOUT(apps.matching("Added live").isEmpty(),5000);
    }
    void preferencesSurviveAtomicReplacements() {
        Meteora::Preferences prefs; QVERIFY(prefs.setValue("panel/height",56));
        QCOMPARE(prefs.value("panel/height").toInt(),56);
        QSignalSpy changes(&prefs,&Meteora::Preferences::changed);
        QSaveFile f(prefs.fileName()); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("[panel]\nheight=44\n"); QVERIFY(f.commit());
        QTRY_VERIFY(changes.count()>0); QCOMPARE(prefs.value("panel/height").toInt(),44);
        changes.clear(); QSaveFile again(prefs.fileName()); QVERIFY(again.open(QIODevice::WriteOnly)); again.write("[panel]\nheight=60\n"); QVERIFY(again.commit());
        QTRY_VERIFY(changes.count()>0); QCOMPARE(prefs.value("panel/height").toInt(),60);
    }
};
int main(int argc,char **argv) {
    QTemporaryDir temp; if(!temp.isValid()) return 1; tempRoot=temp.path();
    qputenv("XDG_DATA_HOME",(tempRoot+"/user").toUtf8()); qputenv("XDG_DATA_DIRS",(tempRoot+"/system").toUtf8());
    qputenv("XDG_CONFIG_HOME",(tempRoot+"/config").toUtf8()); qputenv("XDG_CURRENT_DESKTOP","Meteora");
    QApplication app(argc,argv); CoreTest test; return QTest::qExec(&test,argc,argv);
}
#include "test_core.moc"
