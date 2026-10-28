#ifndef CRYPTO_H
#define CRYPTO_H

#include <QString>
#include <QByteArray>

class crypto {
public:
    QByteArray encrypt(const QString &plainText, const QString &key);
    QString decrypt(const QByteArray &cipherText, const QString &key);
    QByteArray generateKey(int length);
    QByteArray deriveKey(const QString &inputKey, const QByteArray &salt, int iterations);
    QByteArray toBinaryString(const QByteArray &data);
    QByteArray fromBinaryString(const QByteArray &binaryString);
private:
    QByteArray generateIV();
};

#endif // CRYPTO_H
