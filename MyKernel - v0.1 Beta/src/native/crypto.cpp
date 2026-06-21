// crypto.cpp
// Implementation of PayloadCrypto using OpenSSL's EVP API for both
// AES-256-CBC and SHA-256. OpenSSL is used because it's the de-facto
// standard, well-audited, and available via package managers on every
// platform this project targets (see BUILD.md for install instructions).

#include "crypto.hpp"

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <iomanip>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <fstream>

namespace mykernel {

namespace {

// Reads the AES key from a key file, or generates a deterministic
// development fallback so the module is usable before key provisioning
// is finalized. See BUILD.md for how to provide a real key file.
//
// NOTE (v0.1 known limitation): production builds should replace this
// with a properly provisioned key. This embedded/dev-fallback approach
// is documented as a deliberate v0.1 limitation in
// Documentation/03-justmko-format.md §3.5.
Bytes load_or_create_dev_key() {
    const char* key_path = std::getenv("MYKERNEL_AES_KEY_PATH");
    std::string path = key_path ? key_path : "mykernel.key";

    std::ifstream f(path, std::ios::binary);
    if (f.good()) {
        Bytes key((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        if (key.size() == 32) {
            return key;
        }
        // Wrong size on disk — fall through and regenerate below.
    }

    // Generate a fresh random 32-byte key and persist it so repeated runs
    // of the same kernel installation use the same key.
    Bytes key(32);
    if (RAND_bytes(key.data(), static_cast<int>(key.size())) != 1) {
        throw std::runtime_error("PayloadCrypto: failed to generate AES key (RAND_bytes)");
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out.good()) {
        out.write(reinterpret_cast<const char*>(key.data()), static_cast<std::streamsize>(key.size()));
    }
    return key;
}

void throw_openssl_error(const std::string& context) {
    unsigned long err = ERR_get_error();
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    throw DecryptionError(context + ": " + buf);
}

} // namespace

PayloadCrypto::PayloadCrypto() {
    key_ = load_or_create_dev_key();
}

PayloadCrypto::~PayloadCrypto() = default;

std::string PayloadCrypto::sha256_hex(const Bytes& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        throw std::runtime_error("PayloadCrypto: EVP_MD_CTX_new failed");
    }

    bool ok = EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1
        && EVP_DigestUpdate(ctx, data.data(), data.size()) == 1
        && EVP_DigestFinal_ex(ctx, digest, nullptr) == 1;

    EVP_MD_CTX_free(ctx);

    if (!ok) {
        throw std::runtime_error("PayloadCrypto: SHA-256 computation failed");
    }

    std::ostringstream oss;
    for (unsigned char b : digest) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    }
    return oss.str();
}

bool PayloadCrypto::verify_sha256(const Bytes& data, const std::string& expected_hex) {
    std::string actual = sha256_hex(data);
    std::string expected_lower = expected_hex;
    std::transform(expected_lower.begin(), expected_lower.end(), expected_lower.begin(), ::tolower);
    return actual == expected_lower;
}

Bytes PayloadCrypto::decrypt(const Bytes& ciphertext) const {
    if (ciphertext.size() <= AES_BLOCK_SIZE) {
        throw DecryptionError("PayloadCrypto: ciphertext too short to contain an IV + data");
    }

    // Layout: [16-byte IV][encrypted data (PKCS7 padded)]
    const uint8_t* iv = ciphertext.data();
    const uint8_t* enc_data = ciphertext.data() + AES_BLOCK_SIZE;
    int enc_len = static_cast<int>(ciphertext.size() - AES_BLOCK_SIZE);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw DecryptionError("PayloadCrypto: EVP_CIPHER_CTX_new failed");
    }

    Bytes plaintext(enc_len);
    int out_len1 = 0, out_len2 = 0;

    bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_.data(), iv) == 1;
    if (ok) {
        ok = EVP_DecryptUpdate(ctx, plaintext.data(), &out_len1, enc_data, enc_len) == 1;
    }
    if (ok) {
        // This is where a wrong key or corrupted data typically surfaces,
        // since PKCS7 padding validation happens here.
        ok = EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len1, &out_len2) == 1;
    }

    if (!ok) {
        EVP_CIPHER_CTX_free(ctx);
        throw_openssl_error("PayloadCrypto: AES decryption failed (wrong key or corrupted payload)");
    }

    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(static_cast<size_t>(out_len1 + out_len2));
    return plaintext;
}

Bytes PayloadCrypto::encrypt(const Bytes& plaintext) const {
    Bytes iv(AES_BLOCK_SIZE);
    if (RAND_bytes(iv.data(), static_cast<int>(iv.size())) != 1) {
        throw std::runtime_error("PayloadCrypto: failed to generate IV (RAND_bytes)");
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw std::runtime_error("PayloadCrypto: EVP_CIPHER_CTX_new failed");
    }

    // Plaintext + up to one full block of PKCS7 padding.
    Bytes ciphertext(plaintext.size() + AES_BLOCK_SIZE);
    int out_len1 = 0, out_len2 = 0;

    bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr, key_.data(), iv.data()) == 1;
    if (ok) {
        ok = EVP_EncryptUpdate(ctx, ciphertext.data(), &out_len1, plaintext.data(),
                                static_cast<int>(plaintext.size())) == 1;
    }
    if (ok) {
        ok = EVP_EncryptFinal_ex(ctx, ciphertext.data() + out_len1, &out_len2) == 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    if (!ok) {
        throw std::runtime_error("PayloadCrypto: AES encryption failed");
    }

    ciphertext.resize(static_cast<size_t>(out_len1 + out_len2));

    // Prepend the IV so decrypt() can recover it.
    Bytes result;
    result.reserve(iv.size() + ciphertext.size());
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    return result;
}

Bytes PayloadCrypto::verify_and_decrypt(const Bytes& ciphertext, const std::string& expected_sha256) const {
    if (!verify_sha256(ciphertext, expected_sha256)) {
        throw IntegrityError("PayloadCrypto: SHA-256 mismatch — payload is corrupted or has been tampered with");
    }
    return decrypt(ciphertext);
}

} // namespace mykernel
