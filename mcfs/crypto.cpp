#include "crypto.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <QDebug>


QByteArray crypto::deriveKey(const QString &inputKey, const QByteArray &salt, int iterations)
{
    QByteArray key(32, '\0'); // 32 bytes = 256 bits
    QString password = inputKey; // Convert input key to QString

    // PBKDF2_HMAC_SHA256 (password, salt, iterations, key length, output key)
    int result = PKCS5_PBKDF2_HMAC_SHA1(password.toUtf8().data(), password.size(),
                                        reinterpret_cast<const unsigned char*>(salt.data()), salt.size(),
                                        iterations, 32, reinterpret_cast<unsigned char*>(key.data()));

    if (result == 0) {
        //qDebug() << "PBKDF2 key derivation failed.";
        return QByteArray();
    }

    return key;
}

QByteArray crypto::generateKey(int length)
{
    QByteArray key(length, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(key.data()), length) != 1) {
        //qDebug() << "Key generation failed.";
        return QByteArray();
    }
    return key;
}

QByteArray crypto::generateIV()
{
    QByteArray iv(EVP_MAX_IV_LENGTH, '\0');
    if (!RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), iv.size())) {
        //qDebug() << "Failed to generate IV.";
        return QByteArray();
    }
    return iv;
}

QByteArray crypto::encrypt(const QString &plainText, const QString &key)
{
    QByteArray plaintext = plainText.toUtf8();
    QByteArray iv = generateIV();

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        //qDebug() << "Failed to create EVP_CIPHER_CTX.";
        return QByteArray();
    }

    if (1 != EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                                reinterpret_cast<const unsigned char*>(key.toUtf8().data()),
                                reinterpret_cast<const unsigned char*>(iv.data()))) {
        //qDebug() << "Failed to initialize encryption.";
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    int len;
    QByteArray ciphertext(plaintext.size() + EVP_CIPHER_block_size(EVP_aes_256_cbc()), '\0');

    if (1 != EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()), &len,
                               reinterpret_cast<const unsigned char*>(plaintext.data()), plaintext.size())) {
        //qDebug() << "Failed to encrypt.";
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    int ciphertext_len = len;

    if (1 != EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(ciphertext.data()) + len, &len)) {
        //qDebug() << "Failed to finalize encryption.";
        EVP_CIPHER_CTX_free(ctx);
        return QByteArray();
    }

    ciphertext_len += len;
    ciphertext.resize(ciphertext_len);

    EVP_CIPHER_CTX_free(ctx);

    return iv + ciphertext;
}

QString crypto::decrypt(const QByteArray &cipherText, const QString &key)
{
    // Ensure there is enough data for IV and ciphertext
    if (cipherText.size() < EVP_CIPHER_block_size(EVP_aes_256_cbc())) {
        //qDebug() << "Ciphertext too short.";
        return QString();
    }

    // Extract IV and ciphertext
    QByteArray iv = cipherText.left(EVP_CIPHER_block_size(EVP_aes_256_cbc()));
    QByteArray ciphertext = cipherText.mid(EVP_CIPHER_block_size(EVP_aes_256_cbc()));

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        //qDebug() << "Failed to create EVP_CIPHER_CTX.";
        return QString();
    }

    if (1 != EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                                reinterpret_cast<const unsigned char*>(key.toUtf8().data()),
                                reinterpret_cast<const unsigned char*>(iv.data()))) {
        //qDebug() << "Failed to initialize decryption.";
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    int len;
    QByteArray plaintext(ciphertext.size(), '\0');

    if (1 != EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(plaintext.data()), &len,
                               reinterpret_cast<const unsigned char*>(ciphertext.data()), ciphertext.size())) {
        //qDebug() << "Failed to decrypt.";
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    int plaintext_len = len;

    // Finalize the decryption
    if (1 != EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plaintext.data()) + len, &len)) {
        //qDebug() << "Failed to finalize decryption.";
        EVP_CIPHER_CTX_free(ctx);
        return QString();
    }

    plaintext_len += len;
    plaintext.resize(plaintext_len);

    EVP_CIPHER_CTX_free(ctx);

    return QString::fromUtf8(plaintext);
}

// Helper functions to convert to/from binary string
QByteArray crypto::toBinaryString(const QByteArray &data)
{
    QByteArray binaryString;
    for (char byte : data)
    {
        binaryString.append(QString::number(static_cast<unsigned char>(byte), 2).rightJustified(8, '0').toUtf8());
    }
    return binaryString;
}

QByteArray crypto::fromBinaryString(const QByteArray &binaryString)
{
    QByteArray data;
    for (int i = 0; i < binaryString.size(); i += 8)
    {
        bool ok;
        data.append(static_cast<char>(binaryString.mid(i, 8).toInt(&ok, 2)));
    }
    return data;
}
