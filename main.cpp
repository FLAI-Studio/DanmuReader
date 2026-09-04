#include "mainwindow.h"
#include <QApplication>
#include <QProcess>
#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>

bool isRunAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin;
}

// Windows API 拿路径
QString getExePath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return QString::fromWCharArray(path);
}
#endif

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    if (!isRunAsAdmin()) {
        QString program = getExePath();
        QString workDir = QFileInfo(program).absolutePath();

        QProcess::execute("powershell", QStringList()
                                            << "-WindowStyle" << "Hidden"
                                            << "-Command"
                                            << QString("Start-Process -FilePath \"%1\" -Verb RunAs -WorkingDirectory \"%2\"")
                                                   .arg(program, QDir::toNativeSeparators(workDir)));

        return 0;
    }
#endif

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
