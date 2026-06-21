// crypto.hpp
// PayloadCrypto — owns ALL cryptographic operations for MyKernel.
// AES-256-CBC for payload encryption/decryption, SHA-256 for integrity
// verification. This is the only place actual crypto logic lives, per
// Documentation/09-project-structure.md §9.3.

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>

namespace mykernel {

// Thrown when SHA-256 verification fails (file corrupted or tampered with).
class IntegrityError : public std::runtime_error {
public:
    explicit IntegrityError(const std::string& msg) : std::runtime_error(msg) {}
};

// Thrown when AES decryption fails (bad key, bad padding, malformed data).
class DecryptionError : public std::runtime_error {
public:
    explicit DecryptionError(const std::string& msg) : std::runtime_error(msg) {}
};

using Bytes = std::vector<uint8_t>;

class PayloadCrypto {
public:
    PayloadCrypto();
    ~PayloadCrypto();

    // Computes the SHA-256 hash of `data` and returns it as a lowercase
    // hex string (64 characters). Used to produce/verify payload.sha256.
    static std::string sha256_hex(const Bytes& data);

    // Compares `data`'s SHA-256 against `expected_hex` (case-insensitive).
    // Returns true if they match.
    static bool verify_sha256(const Bytes& data, const std::string& expected_hex);

    // Decrypts `ciphertext` using AES-256-CBC with the kernel's embedded
    // key. Expects the first 16 bytes of `ciphertext` to be the IV,
    // followed by the encrypted data (PKCS7 padded).
    // Throws DecryptionError on failure (wrong key/corrupted data/bad padding).
    Bytes decrypt(const Bytes& ciphertext) const;

    // Encrypts `plaintext` using AES-256-CBC with the kernel's embedded
    // key. Generates a random IV and prepends it to the output, matching
    // the format `decrypt()` expects.
    // Used by mk_bmd at build time to produce payload.bin.
    Bytes encrypt(const Bytes& plaintext) const;

    // Convenience: verify + decrypt in one call. Throws IntegrityError
    // first if the hash doesn't match, before ever attempting decryption.
    Bytes verify_and_decrypt(const Bytes& ciphertext, const std::string& expected_sha256) const;

private:
    // 32-byte AES-256 key. v0.1: embedded in the binary (see
    // Documentation/03-justmko-format.md §3.5 "Honest Note on Protection
    // Level" — this is a deliberate, documented v0.1 limitation).
    Bytes key_;

    static constexpr size_t AES_KEY_SIZE = 32;  // 256 bits
    static constexpr size_t AES_BLOCK_SIZE = 16; // 128 bits
};

} // namespace mykernel
