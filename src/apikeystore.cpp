#include "apikeystore.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dpapi.h>

namespace {
const QByteArray fileHeader = QByteArrayLiteral("RabbitRoll.APIKEY.DPAPI.v1\n");
constexpr qsizetype maximumKeyLength = 4096;
constexpr qint64 maximumFileSize = 64 * 1024;

bool validKey(const QString& key)
{
    if (key.size() > maximumKeyLength) return false;
    for (const auto character : key)
        if (character.unicode() < 0x21 || character.unicode() > 0x7e) return false;
    return true;
}
}

ApiKeyStore::ApiKeyStore(const QString& filePath) : _filePath(filePath) {}

bool ApiKeyStore::load(QString& key, QString& error) const
{
    key.clear();
    error.clear();
    if (!QFileInfo::exists(_filePath)) return true;
    QFile file(_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error = "无法读取已保存的 API key，请检查程序目录的访问权限。";
        return false;
    }
    if (file.size() > maximumFileSize) {
        error = "API key 文件异常，请重新输入并保存。";
        return false;
    }
    auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError || !bytes.startsWith(fileHeader) || bytes.size() <= fileHeader.size()) {
        error = "API key 文件格式无效，请重新输入并保存。";
        return false;
    }
    DATA_BLOB encrypted{static_cast<DWORD>(bytes.size() - fileHeader.size()),
                        reinterpret_cast<BYTE*>(bytes.data() + fileHeader.size())};
    DATA_BLOB plaintext{};
    if (!CryptUnprotectData(&encrypted, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &plaintext)) {
        error = "无法解密 API key，可能已更换 Windows 账户、电脑或文件已损坏；请重新输入。";
        return false;
    }
    const bool sizeValid = plaintext.cbData > 0 && plaintext.cbData <= maximumKeyLength;
    if (sizeValid) key = QString::fromUtf8(reinterpret_cast<const char*>(plaintext.pbData), plaintext.cbData);
    SecureZeroMemory(plaintext.pbData, plaintext.cbData);
    LocalFree(plaintext.pbData);
    if (!sizeValid || key.isEmpty() || !validKey(key)) {
        key.clear();
        error = "API key 文件内容无效，请重新输入并保存。";
        return false;
    }
    return true;
}

bool ApiKeyStore::save(const QString& key, QString& error) const
{
    error.clear();
    const auto normalized = key.trimmed();
    if (!validKey(normalized)) {
        error = "API key 不能包含空格、换行或非英文字符，且不能超过 4096 个字符。";
        return false;
    }
    if (normalized.isEmpty()) {
        if (QFileInfo::exists(_filePath) && !QFile::remove(_filePath)) {
            error = "清除失败，请检查 API key 文件是否被占用或目录是否可写。";
            return false;
        }
        return true;
    }
    auto plaintext = normalized.toUtf8();
    DATA_BLOB input{static_cast<DWORD>(plaintext.size()), reinterpret_cast<BYTE*>(plaintext.data())};
    DATA_BLOB encrypted{};
    // Default DPAPI scope binds the ciphertext to the current Windows user.
    const bool protectedData = CryptProtectData(&input, L"RabbitRoll API key", nullptr, nullptr, nullptr,
                                                CRYPTPROTECT_UI_FORBIDDEN, &encrypted);
    SecureZeroMemory(plaintext.data(), plaintext.size());
    if (!protectedData) {
        error = "Windows 无法加密 API key，原文件未修改。";
        return false;
    }
    QByteArray bytes = fileHeader;
    bytes.append(reinterpret_cast<const char*>(encrypted.pbData), encrypted.cbData);
    LocalFree(encrypted.pbData);
    QSaveFile file(_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        error = "无法保存 API key，请将程序放在可写目录，并检查文件占用。";
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        file.cancelWriting();
        error = "保存 API key 失败，原文件未修改；请检查目录权限或文件占用。";
        return false;
    }
    return true;
}
