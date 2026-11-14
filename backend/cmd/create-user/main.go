package main

import (
	"database/sql"
	"flag"
	"fmt"
	"log"
	"os"
	"strings"

	"backend/internal/config"
	"backend/internal/crypto"
	"backend/internal/database"
)

func main() {
	usernameFlag := flag.String("username", "", "Username operator baru (3-64 karakter, tanpa spasi)")
	passwordFlag := flag.String("password", "", "Kata sandi operator (minimal 8 karakter)")
	roleFlag := flag.String("role", "operator", "Role pengguna (default: operator)")
	updateFlag := flag.Bool("update", false, "Perbarui kata sandi dan role jika username sudah ada")
	flag.Parse()

	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("Gagal memuat konfigurasi: %v", err)
	}

	db, err := database.Connect(cfg)
	if err != nil {
		log.Fatalf("Gagal terhubung ke database MySQL: %v", err)
	}
	defer db.Close()

	action, err := upsertUser(db, *usernameFlag, *passwordFlag, *roleFlag, *updateFlag)
	if err != nil {
		fmt.Fprintln(os.Stderr, "Error:", err)
		os.Exit(1)
	}

	fmt.Printf("Sukses: akun '%s' (role: %s) berhasil %s.\n", strings.TrimSpace(*usernameFlag), strings.TrimSpace(*roleFlag), action)
}

func upsertUser(db *sql.DB, rawUsername, rawPassword, rawRole string, allowUpdate bool) (string, error) {
	username := strings.TrimSpace(rawUsername)
	if len(username) < 3 || len(username) > 64 || strings.ContainsAny(username, " \t\r\n") {
		return "", fmt.Errorf("username wajib 3-64 karakter tanpa spasi")
	}

	if len(rawPassword) < 8 || len(rawPassword) > 128 {
		return "", fmt.Errorf("kata sandi wajib 8-128 karakter")
	}

	role := strings.TrimSpace(rawRole)
	if role == "" {
		role = "operator"
	}
	if role != "operator" {
		return "", fmt.Errorf("role tidak valid '%s' (hanya mendukung 'operator')", role)
	}

	hash, err := crypto.HashPassword(rawPassword)
	if err != nil {
		return "", fmt.Errorf("gagal melakukan hash kata sandi: %w", err)
	}

	var existingID int64
	err = db.QueryRow("SELECT id FROM users WHERE username = ? LIMIT 1", username).Scan(&existingID)
	if err != nil && err != sql.ErrNoRows {
		return "", fmt.Errorf("gagal memeriksa user yang ada: %w", err)
	}

	if err == nil {
		if !allowUpdate {
			return "", fmt.Errorf("username '%s' sudah terdaftar (tambahkan flag -update untuk mengubah kata sandinya)", username)
		}
		if _, err := db.Exec("UPDATE users SET password_hash = ?, role = ? WHERE id = ?", hash, role, existingID); err != nil {
			return "", fmt.Errorf("gagal memperbarui user: %w", err)
		}
		database.WriteAuditLog(db, "cli", "config_change", "Kata sandi akun operator diperbarui via CLI: "+username)
		return "diperbarui", nil
	}

	if _, err := db.Exec(
		"INSERT INTO users (username, password_hash, role) VALUES (?, ?, ?)",
		username, hash, role,
	); err != nil {
		return "", fmt.Errorf("gagal membuat user baru: %w", err)
	}

	database.WriteAuditLog(db, "cli", "config_change", "Akun operator baru ditambahkan via CLI: "+username)
	return "dibuat", nil
}
