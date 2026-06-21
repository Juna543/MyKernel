# 3. `.justmko` Format

## 3.1 Definition

`.justmko` (**Just MyKernel OS**) is the file format for an OS Instance — conceptually equivalent to a `.iso` on a traditional OS. Technically, it's a **ZIP archive** containing open metadata plus an encrypted OS payload.

## 3.2 File Structure

```
mycoolos.justmko   (base format: ZIP)
│
├── manifest.json       ← open, readable without decryption
│     - OS name
│     - version
│     - package (Work / Code / Home)
│     - source/origin ("load from where")
│     - description
│
├── payload.bin          ← encrypted (AES)
│     - Binary Config: filesystem, OS definitions, core data
│
└── payload.sha256        ← SHA-256 hash of payload.bin, used for integrity
                             verification before decryption
```

## 3.3 Role of Each Part

| Part | Role | Readable directly? |
|---|---|---|
| `manifest.json` | OS Instance identity & metadata | Yes — open, not encrypted |
| `payload.bin` | Core OS data (filesystem, binary config) | No — only decryptable by MyKernel's C++ module |
| `payload.sha256` | Integrity seal, ensures `payload.bin` hasn't been corrupted or tampered with | Yes — but the value is only useful for verification |

## 3.4 Why Not Just Base64 + SHA?

Early in the design discussion, it was worth asking whether `Base64` and `SHA` alone would be enough to protect the contents of `.justmko`. It's important to be precise about what each one actually does:

- **Base64** is not encryption — it's just an *encoding* that makes binary data safe to embed inside text-based structures. Anyone can decode Base64 in seconds; it does not protect the file's contents.
- **SHA-256** is not encryption either — it's a one-way *hash* used to verify a file's authenticity/integrity. It confirms the file hasn't changed, but it doesn't hide the content.

Since there's a real requirement that `.justmko` contents should **not be easily pirated or copied**, the main payload (`payload.bin`) is **encrypted with AES** rather than merely Base64-encoded.

## 3.5 Cryptographic Scheme

- **Algorithm**: AES (symmetric) — a single key is used for both encryption and decryption.
- **Key location**: stored inside MyKernel itself (not distributed externally).
- **Decryption process**: performed by the native C++ module (`PayloadCrypto`), not by Python — see [02-architecture.md](02-architecture.md).
- **Integrity verification**: SHA-256 is computed from `payload.bin` and checked against `payload.sha256` **before** decryption runs. If they don't match, the `run` process stops immediately.

### Honest Note on Protection Level

AES with a key embedded inside MyKernel provides protection that **raises the bar**, but doesn't make piracy **impossible**. Anyone with access to the MyKernel binary could, in theory, attempt to extract the key through reverse engineering. This is a reasonable trade-off for a v0.1/Beta stage. If stronger protection is needed later, future options include:

- An online license server (per-user / per-device key validation).
- A hardware-bound key (tied to a specific device).

These are not implemented in v0.1 — noted here as considerations for future versions. See [08-roadmap.md](08-roadmap.md).

## 3.6 Verification & Decryption Flow (Summary)

```
1. Compute SHA-256 of payload.bin
2. Compare against the value in payload.sha256
   ├── Mismatch → reject, stop execution (file considered corrupted/tampered)
   └── Match    → proceed to step 3
3. Decrypt payload.bin using AES (key stored inside MyKernel)
4. Return the decrypted result to the Python Layer for the boot process
```

---
Continue to [04-packages.md](04-packages.md) for the Work/Code/Home package breakdown.
