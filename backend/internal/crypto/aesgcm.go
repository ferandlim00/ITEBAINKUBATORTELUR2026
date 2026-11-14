package crypto

import (
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"encoding/base64"
	"errors"
	"io"
)

// EncryptAESGCM encrypts plaintext using AES-256-GCM with a fresh random 12-byte nonce.
func EncryptAESGCM(key []byte, plaintext string) (ciphertextB64 string, nonceB64 string, err error) {
	if len(key) != 32 {
		return "", "", errors.New("kunci AES-256 harus berukuran 32 byte")
	}

	block, err := aes.NewCipher(key)
	if err != nil {
		return "", "", err
	}

	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return "", "", err
	}

	nonce := make([]byte, gcm.NonceSize())
	if _, err := io.ReadFull(rand.Reader, nonce); err != nil {
		return "", "", err
	}

	sealed := gcm.Seal(nil, nonce, []byte(plaintext), nil)
	return base64.StdEncoding.EncodeToString(sealed), base64.StdEncoding.EncodeToString(nonce), nil
}

// DecryptAESGCM decrypts AES-256-GCM ciphertext using its base64-encoded nonce.
func DecryptAESGCM(key []byte, ciphertextB64, nonceB64 string) (string, error) {
	if len(key) != 32 {
		return "", errors.New("kunci AES-256 harus berukuran 32 byte")
	}

	ciphertext, err := base64.StdEncoding.DecodeString(ciphertextB64)
	if err != nil {
		return "", err
	}
	nonce, err := base64.StdEncoding.DecodeString(nonceB64)
	if err != nil {
		return "", err
	}

	block, err := aes.NewCipher(key)
	if err != nil {
		return "", err
	}
	gcm, err := cipher.NewGCM(block)
	if err != nil {
		return "", err
	}
	if len(nonce) != gcm.NonceSize() {
		return "", errors.New("panjang nonce tidak valid")
	}

	plain, err := gcm.Open(nil, nonce, ciphertext, nil)
	if err != nil {
		return "", err
	}
	return string(plain), nil
}
