#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

// The envelope operates on bytes. Text decoding and schema checks happen only
// after authentication, and never write to the source file.
namespace RschFile {
enum class Error {
    None, Read, Encoding, JsonSyntax, Schema, Authentication,
    UnsupportedVersion, Crypto, Write
};

struct Result {
    Error error = Error::None;
    QString message;
    QByteArray plaintext;
    QByteArray utf8;
    QJsonObject root;
    explicit operator bool() const { return error == Error::None; }
};

bool encrypt(const QByteArray& plaintext, QByteArray& envelope, QString& message);
Result decrypt(const QByteArray& envelope);
Result decode(const QByteArray& fileBytes);
Result read(const QString& path);
Result write(const QString& path, const QByteArray& plaintext);
QString validate(const QJsonObject& root);
}
