package crypto

import (
	"strings"
	"testing"
)

func TestArgon2HashAndVerify(t *testing.T) {
	hash, err := HashPassword("operator12345")
	if err != nil {
		t.Fatalf("HashPassword error: %v", err)
	}
	if !strings.HasPrefix(hash, "$argon2id$") {
		t.Fatalf("expected argon2id PHC string, got %q", hash)
	}

	ok, err := VerifyPassword("operator12345", hash)
	if err != nil || !ok {
		t.Fatalf("expected valid password verification, ok=%v err=%v", ok, err)
	}

	ok, err = VerifyPassword("salah-password", hash)
	if err != nil {
		t.Fatalf("unexpected error on wrong password: %v", err)
	}
	if ok {
		t.Fatalf("expected wrong password to fail verification")
	}
}

func TestAES256GCMEncryptDecrypt(t *testing.T) {
	key := make([]byte, 32)
	for i := range key {
		key[i] = byte(i + 1)
	}

	ct1, nonce1, err := EncryptAESGCM(key, "kata-sandi-wifi-aman")
	if err != nil {
		t.Fatalf("EncryptAESGCM failed: %v", err)
	}
	ct2, nonce2, err := EncryptAESGCM(key, "kata-sandi-wifi-aman")
	if err != nil {
		t.Fatalf("EncryptAESGCM second call failed: %v", err)
	}
	if nonce1 == nonce2 || ct1 == ct2 {
		t.Fatalf("expected unique nonce and ciphertext on each call")
	}

	plain, err := DecryptAESGCM(key, ct1, nonce1)
	if err != nil {
		t.Fatalf("DecryptAESGCM failed: %v", err)
	}
	if plain != "kata-sandi-wifi-aman" {
		t.Fatalf("expected decrypted plaintext to match, got %q", plain)
	}
}
