#include "rschfile.h"

#include <QDate>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringDecoder>
#include <QTime>
#include <QtEndian>
#include <cmath>
#include <windows.h>
#include <bcrypt.h>

namespace {
const QByteArray magic = QByteArray::fromHex("52525343480d0a1a");
constexpr qsizetype headerSize = 28;
constexpr qsizetype tagSize = 16;
constexpr qsizetype maxPlaintext = 64 * 1024 * 1024;

// Shared application key, NOT a user secret or protection against reverse
// engineering. Keep key ID 1 available when adding a future key ID.
const QByteArray sharedKey = QByteArray::fromHex(
    "a67b9472a58d0952fe4420693c0c9e90e9efb43ec5bc98859bedc9af5cd99774");

RschFile::Result failure(RschFile::Error error, const QString& message) {
    RschFile::Result result;
    result.error = error;
    result.message = message;
    return result;
}

struct AesKey {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_KEY_HANDLE key = nullptr;
    bool init() {
        return BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0) >= 0
            && BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE,
                reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                sizeof(BCRYPT_CHAIN_MODE_GCM), 0) >= 0
            && BCryptGenerateSymmetricKey(algorithm, &key, nullptr, 0,
                reinterpret_cast<PUCHAR>(const_cast<char*>(sharedKey.constData())),
                static_cast<ULONG>(sharedKey.size()), 0) >= 0;
    }
    ~AesKey() {
        if (key) BCryptDestroyKey(key);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    }
};

bool gcm(bool encrypt, QByteArray input, QByteArray header, QByteArray& tag, QByteArray& output) {
    AesKey aes;
    if (!aes.init()) return false;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
    BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce = reinterpret_cast<PUCHAR>(header.data() + 12);
    info.cbNonce = 12;
    info.pbAuthData = reinterpret_cast<PUCHAR>(header.data());
    info.cbAuthData = static_cast<ULONG>(header.size());
    info.pbTag = reinterpret_cast<PUCHAR>(tag.data());
    info.cbTag = tagSize;
    QByteArray candidate(input.size() + 1, '\0');
    ULONG written = 0;
    const auto operation = encrypt ? BCryptEncrypt : BCryptDecrypt;
    const NTSTATUS status = operation(aes.key, reinterpret_cast<PUCHAR>(input.data()),
        static_cast<ULONG>(input.size()), &info, nullptr, 0,
        reinterpret_cast<PUCHAR>(candidate.data()), static_cast<ULONG>(candidate.size()), &written, 0);
    if (status < 0 || written != input.size()) {
        SecureZeroMemory(candidate.data(), static_cast<SIZE_T>(candidate.size()));
        return false;
    }
    candidate.resize(written);
    output = std::move(candidate);
    return true;
}

bool encryptedMarker(const QByteArray& bytes) {
    return bytes.startsWith("RRSCH")
        || (!bytes.isEmpty() && bytes.size() < magic.size() && magic.startsWith(bytes));
}

bool strictText(const QByteArray& bytes, QByteArray& utf8) {
    // Reject UTF-32 before interpreting FF FE as UTF-16 LE.
    if (bytes.startsWith(QByteArray::fromHex("fffe0000"))
        || bytes.startsWith(QByteArray::fromHex("0000feff"))) return false;
    const bool le = bytes.startsWith(QByteArray::fromHex("fffe"));
    const bool be = bytes.startsWith(QByteArray::fromHex("feff"));
    if (le || be) {
        if ((bytes.size() - 2) % 2 != 0) return false;
        QString text;
        text.reserve((bytes.size() - 2) / 2);
        bool awaitingLow = false;
        for (qsizetype offset = 2; offset < bytes.size(); offset += 2) {
            const auto* data = reinterpret_cast<const uchar*>(bytes.constData() + offset);
            const ushort code = le ? qFromLittleEndian<ushort>(data) : qFromBigEndian<ushort>(data);
            const bool high = QChar::isHighSurrogate(code);
            const bool low = QChar::isLowSurrogate(code);
            if ((awaitingLow && !low) || (!awaitingLow && low)) return false;
            awaitingLow = high;
            text.append(QChar(code));
        }
        if (awaitingLow) return false;
        utf8 = text.toUtf8();
        return true;
    }
    QByteArray payload = bytes;
    if (payload.startsWith(QByteArray::fromHex("efbbbf"))) payload.remove(0, 3);
    QStringDecoder decoder(QStringDecoder::Utf8,
        QStringConverter::Flag::Stateless | QStringConverter::Flag::ConvertInitialBom);
    const QString text = decoder(payload);
    // Round-trip also rejects incomplete sequences and any lossy conversion.
    if (decoder.hasError() || text.toUtf8() != payload) return false;
    utf8 = payload;
    return true;
}

bool integers(const QJsonValue& value, int maximum) {
    if (!value.isArray() || value.toArray().isEmpty()) return false;
    for (const auto& item : value.toArray()) {
        const double number = item.toDouble(-1);
        if (!item.isDouble() || number < 1 || number > maximum || std::floor(number) != number) return false;
    }
    return true;
}

bool validTime(const QString& value) {
    const QTime time = QTime::fromString(value, "HH:mm");
    return time.isValid() && time.toString("HH:mm") == value;
}

bool timeRange(const QString& value) {
    const auto parts = value.split('-');
    return parts.size() == 2 && validTime(parts[0]) && validTime(parts[1]) && parts[0] < parts[1];
}
}

namespace RschFile {
bool encrypt(const QByteArray& plaintext, QByteArray& envelope, QString& message) {
    envelope.clear();
    message.clear();
    if (plaintext.size() > maxPlaintext) {
        message = "课表超过加密格式的 64 MiB 大小限制。";
        return false;
    }
    QByteArray header = magic;
    header.append(QByteArray::fromHex("01010100")); // version, AES-GCM, key ID, flags
    header.resize(headerSize);
    if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(header.data() + 12),
            12, BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) {
        message = "无法生成安全随机数，课表未导出。";
        return false;
    }
    qToBigEndian<quint32>(static_cast<quint32>(plaintext.size()), header.data() + 24);
    QByteArray tag(tagSize, '\0'), ciphertext;
    if (!gcm(true, plaintext, header, tag, ciphertext)) {
        message = "课表加密失败，文件未保存。";
        return false;
    }
    envelope = header + ciphertext + tag;
    return true;
}

Result decrypt(const QByteArray& envelope) {
    const auto invalid = [] { return failure(Error::Authentication, "加密认证失败或课表文件已损坏，当前数据未更改。"); };
    if (envelope.size() < magic.size() + 1 || !envelope.startsWith(magic)) return invalid();
    if (static_cast<uchar>(envelope[8]) != 1)
        return failure(Error::UnsupportedVersion, "不支持此加密课表格式版本，请升级 RabbitRoll。");
    if (envelope.size() < headerSize + tagSize || envelope[9] != 1
        || envelope[10] != 1 || envelope[11] != 0) return invalid();
    const quint32 length = qFromBigEndian<quint32>(envelope.constData() + 24);
    if (length > maxPlaintext || envelope.size() != headerSize + qsizetype(length) + tagSize) return invalid();
    QByteArray tag = envelope.right(tagSize), plaintext;
    if (!gcm(false, envelope.mid(headerSize, length), envelope.first(headerSize), tag, plaintext)) return invalid();
    Result result;
    result.plaintext = std::move(plaintext);
    return result;
}

QString validate(const QJsonObject& root) {
    if (root["app"].toString() != "rabbitroll") return "数据格式不符合要求：app 必须为 rabbitroll。";
    const QString type = root["dataType"].toString();
    if (type == "schedule") {
        if (!root["scheduleData"].isObject()) return "缺少必要字段或类型错误：scheduleData 必须是对象。";
        const auto days = root["scheduleData"].toObject();
        for (auto day = days.begin(); day != days.end(); ++day) {
            const auto date = QDate::fromString(day.key(), Qt::ISODate);
            if (!date.isValid() || date.toString(Qt::ISODate) != day.key() || !day.value().isObject())
                return "时间表数据不符合要求：日期或日期内容类型无效。";
            const auto times = day.value().toObject();
            for (auto time = times.begin(); time != times.end(); ++time) {
                if (!timeRange(time.key()) || !time.value().isBool()) return "时间表数据不符合要求：时间范围或布尔值无效。";
            }
        }
        return {};
    }
    if (type != "course_schedule") return "数据类型不符合要求：仅支持 course_schedule 和 schedule。";
    if (root["school"].toString() != "jwxt.wit.edu.cn") return "学校不符合要求：当前仅支持 jwxt.wit.edu.cn。";
    if (!root["courses"].isArray() || !root["timetable"].isArray()
        || root["courses"].toArray().isEmpty() || root["timetable"].toArray().isEmpty())
        return "缺少必要字段或类型错误：courses 和 timetable 必须是非空数组。";
    for (const auto& item : root["timetable"].toArray()) {
        const auto block = item.toObject();
        if (!item.isObject() || !validTime(block["startTime"].toString())
            || !validTime(block["endTime"].toString())
            || block["startTime"].toString() >= block["endTime"].toString())
            return "时间块数据不符合要求：起止时间缺失或无效。";
        if (block.contains("sections")) {
            if (!integers(block["sections"], 30)) return "时间块 sections 类型或节次范围无效。";
        } else {
            // Historical combined labels (12, 34, 45, 1011, 1112, ...)
            // are passed unchanged to BackendWorker's existing mapping.
            const auto section = block["section"];
            const QString label = section.toVariant().toString();
            static const QRegularExpression digits("^[1-9][0-9]*$");
            if ((!section.isString() && !section.isDouble()) || !digits.match(label).hasMatch())
                return "时间块缺少有效的 section 或 sections。";
        }
    }
    for (const auto& item : root["courses"].toArray()) {
        const auto course = item.toObject();
        const double day = course["dayOfWeek"].toDouble(-1);
        if (!item.isObject() || !integers(course["weeks"], 64) || !integers(course["sections"], 30)
            || !course["dayOfWeek"].isDouble() || day < 1 || day > 7 || std::floor(day) != day)
            return "课程必要字段缺失或类型、范围错误：weeks、sections、dayOfWeek。";
        for (const auto* field : {"name", "teacher", "location"}) {
            if (course.contains(field) && !course[field].isString()) return QString("课程字段类型错误：%1 必须是文字。").arg(field);
        }
    }
    return {};
}

Result decode(const QByteArray& fileBytes) {
    Result result;
    if (encryptedMarker(fileBytes)) {
        result = decrypt(fileBytes);
        if (!result) return result; // Never fall back to plaintext.
    } else {
        result.plaintext = fileBytes;
    }
    if (!strictText(result.plaintext, result.utf8))
        return failure(Error::Encoding, "文本编码不支持或解码失败。仅支持严格 UTF-8，或带 BOM 的 UTF-16 LE/BE；未替换或忽略任何字符。");
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(result.utf8, &error);
    if (error.error != QJsonParseError::NoError)
        return failure(Error::JsonSyntax, QString("JSON 语法损坏（UTF-8 字节位置 %1）：%2。未尝试修补内容。")
            .arg(error.offset).arg(error.errorString()));
    if (!document.isObject()) return failure(Error::Schema, "数据类型不符合要求：课表顶层必须是对象。");
    result.root = document.object();
    const QString message = validate(result.root);
    if (!message.isEmpty()) return failure(Error::Schema, message);
    return result;
}

Result read(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return failure(Error::Read, "无法读取课表文件：" + file.errorString());
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) return failure(Error::Read, "课表文件读取失败：" + file.errorString());
    return decode(bytes);
}

Result write(const QString& path, const QByteArray& plaintext) {
    QByteArray envelope;
    QString message;
    if (!encrypt(plaintext, envelope, message)) return failure(Error::Crypto, message);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return failure(Error::Write, "无法保存课表文件：" + file.errorString());
    if (file.write(envelope) != envelope.size()) {
        file.cancelWriting();
        return failure(Error::Write, "加密课表未完整写入，保存已取消。");
    }
    if (!file.commit()) return failure(Error::Write, "加密课表保存失败：" + file.errorString());
    return {};
}
}
