package main

import "testing"

func TestUpsertUserValidation(t *testing.T) {
	tests := []struct {
		name     string
		username string
		password string
		role     string
	}{
		{"empty username", "", "PasswordAman123", "operator"},
		{"short username", "ab", "PasswordAman123", "operator"},
		{"username with space", "op 2", "PasswordAman123", "operator"},
		{"short password", "operator2", "1234567", "operator"},
		{"invalid role", "operator2", "PasswordAman123", "admin"},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			if _, err := upsertUser(nil, tc.username, tc.password, tc.role, false); err == nil {
				t.Fatalf("expected validation error for case %q, got nil", tc.name)
			}
		})
	}
}
