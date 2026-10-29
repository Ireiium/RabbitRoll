#include "lock.h"

// Legacy local-settings obfuscation only; this is not an API credential or encryption.
static const QByteArray OBFUSCATION_KEY = QByteArrayLiteral("RabbitRoll2026!@#SecreT");

QByteArray xorTransform(const QByteArray& data, const QByteArray& key) {
    QByteArray result = data;
    for (int i = 0; i < data.size(); ++i)
        result[i] = data[i] ^ key[i % key.size()];
    return result;
}

QByteArray obfuscateKey(const QByteArray& data) {
    return xorTransform(data, OBFUSCATION_KEY).toBase64();
}

QByteArray deobfuscateKey(const QByteArray& data) {
    return xorTransform(QByteArray::fromBase64(data), OBFUSCATION_KEY);
}
