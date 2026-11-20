#include "fileassociation.h"

#include <QDir>
#include <QFileInfo>
#include <string>
#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shlobj.h>

namespace {
QString stringValue(HKEY root, const wchar_t* key, const wchar_t* name) {
    DWORD size = 0;
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) != ERROR_SUCCESS)
        return {};
    std::wstring value(size / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, value.data(), &size) != ERROR_SUCCESS)
        return {};
    return QString::fromWCharArray(value.c_str());
}

LSTATUS writeValue(const wchar_t* key, const wchar_t* name, DWORD type,
                  const QByteArray& data, bool& changed) {
    HKEY handle = nullptr;
    auto status = RegCreateKeyExW(HKEY_CURRENT_USER, key, 0, nullptr, 0,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &handle, nullptr);
    if (status != ERROR_SUCCESS) return status;
    DWORD oldType = 0;
    DWORD size = 0;
    status = RegQueryValueExW(handle, name, nullptr, &oldType, nullptr, &size);
    QByteArray oldData(size, '\0');
    if (status == ERROR_SUCCESS)
        status = RegQueryValueExW(handle, name, nullptr, &oldType,
                                 reinterpret_cast<BYTE*>(oldData.data()), &size);
    if (status != ERROR_SUCCESS || oldType != type || oldData != data) {
        status = RegSetValueExW(handle, name, 0, type,
                               reinterpret_cast<const BYTE*>(data.constData()), DWORD(data.size()));
        if (status == ERROR_SUCCESS) changed = true;
    }
    RegCloseKey(handle);
    return status;
}

LSTATUS writeString(const wchar_t* key, const wchar_t* name, const QString& value, bool& changed) {
    const auto wide = value.toStdWString();
    return writeValue(key, name, REG_SZ,
        QByteArray(reinterpret_cast<const char*>(wide.c_str()), (wide.size() + 1) * sizeof(wchar_t)), changed);
}
}
#endif

bool registerRrollFileAssociation(const QString& executablePath, QString* error) {
    if (error) error->clear();
#ifdef Q_OS_WIN
    if (!QFileInfo(executablePath).isFile()) {
        if (error) *error = QStringLiteral("RabbitRoll 程序文件不存在。");
        return false;
    }
    const auto executable = QDir::toNativeSeparators(QFileInfo(executablePath).absoluteFilePath());
    bool changed = false;
    LSTATUS status = ERROR_SUCCESS;
    auto save = [&](const wchar_t* key, const wchar_t* name, const QString& value) {
        if (status == ERROR_SUCCESS) status = writeString(key, name, value, changed);
    };
    save(L"Software\\Classes\\RabbitRoll.Project", nullptr, QStringLiteral("RabbitRoll 值班表"));
    save(L"Software\\Classes\\RabbitRoll.Project\\DefaultIcon", nullptr,
         QStringLiteral("\"") + executable + QStringLiteral("\",0"));
    // Both paths must be quoted: portable directories and project names can contain spaces.
    save(L"Software\\Classes\\RabbitRoll.Project\\shell\\open\\command", nullptr,
         QStringLiteral("\"") + executable + QStringLiteral("\" \"%1\""));
    if (status == ERROR_SUCCESS)
        status = writeValue(L"Software\\Classes\\.rroll\\OpenWithProgids",
                            L"RabbitRoll.Project", REG_NONE, {}, changed);

    // HKCR includes machine-wide defaults. Windows UserChoice is deliberately untouched.
    const auto existingDefault = stringValue(HKEY_CLASSES_ROOT, L".rroll", nullptr);
    if (existingDefault.isEmpty() || existingDefault == QStringLiteral("RabbitRoll.Project"))
        save(L"Software\\Classes\\.rroll", nullptr, QStringLiteral("RabbitRoll.Project"));
    if (changed) SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    if (status != ERROR_SUCCESS) {
        if (error) *error = QStringLiteral("无法注册 .rroll 文件关联（Windows 错误 %1）。可通过“打开方式”选择 RabbitRoll。").arg(status);
        return false;
    }
#else
    Q_UNUSED(executablePath);
#endif
    return true;
}
