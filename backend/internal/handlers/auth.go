package handlers

import (
	"crypto/rand"
	"database/sql"
	"encoding/hex"
	"strings"
	"time"

	"backend/internal/config"
	"backend/internal/crypto"
	"backend/internal/database"
	"backend/internal/middleware"
	"backend/internal/models"

	"github.com/gofiber/fiber/v3"
)

// AuthHandler handles login, logout, and current session inspection.
type AuthHandler struct {
	db      *sql.DB
	cfg     *config.Config
	limiter *middleware.LoginRateLimiter
}

// NewAuthHandler creates a new AuthHandler.
func NewAuthHandler(db *sql.DB, cfg *config.Config, limiter *middleware.LoginRateLimiter) *AuthHandler {
	return &AuthHandler{db: db, cfg: cfg, limiter: limiter}
}

type loginRequest struct {
	Username string `json:"username"`
	Password string `json:"password"`
}

// Login authenticates an Operator via Argon2id and issues an HttpOnly session cookie.
func (h *AuthHandler) Login(c fiber.Ctx) error {
	ip := c.IP()
	if h.limiter.IsBlocked(ip) {
		database.WriteAuditLog(h.db, "anonim", "access_denied", "Rate limit login terlampaui dari IP "+ip)
		return c.Status(fiber.StatusTooManyRequests).JSON(fiber.Map{
			"message": "Terlalu banyak percobaan login. Silakan coba lagi dalam 1 menit.",
		})
	}

	var req loginRequest
	if err := c.Bind().JSON(&req); err != nil {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Format permintaan login tidak valid.",
		})
	}

	username := strings.TrimSpace(req.Username)
	if username == "" || req.Password == "" {
		h.limiter.RecordFailure(ip)
		return c.Status(fiber.StatusUnauthorized).JSON(fiber.Map{
			"message": "Nama pengguna atau kata sandi salah.",
		})
	}

	var user models.User
	var passwordHash string
	err := h.db.QueryRow(
		"SELECT id, username, password_hash, role FROM users WHERE username = ? LIMIT 1",
		username,
	).Scan(&user.ID, &user.Username, &passwordHash, &user.Role)

	if err != nil {
		h.limiter.RecordFailure(ip)
		database.WriteAuditLog(h.db, username, "access_denied", "Percobaan login gagal dari IP "+ip)
		return c.Status(fiber.StatusUnauthorized).JSON(fiber.Map{
			"message": "Nama pengguna atau kata sandi salah.",
		})
	}

	ok, err := crypto.VerifyPassword(req.Password, passwordHash)
	if err != nil || !ok {
		h.limiter.RecordFailure(ip)
		database.WriteAuditLog(h.db, username, "access_denied", "Kata sandi salah saat login dari IP "+ip)
		return c.Status(fiber.StatusUnauthorized).JSON(fiber.Map{
			"message": "Nama pengguna atau kata sandi salah.",
		})
	}

	rawBytes := make([]byte, 32)
	if _, err := rand.Read(rawBytes); err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal membuat sesi login.",
		})
	}
	rawToken := hex.EncodeToString(rawBytes)
	tokenHash := middleware.HashToken(rawToken)
	expiresAt := time.Now().UTC().Add(24 * time.Hour)

	if _, err := h.db.Exec(
		"INSERT INTO sessions (token_hash, user_id, expires_at) VALUES (?, ?, ?)",
		tokenHash, user.ID, expiresAt,
	); err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal menyimpan sesi ke basis data.",
		})
	}

	// Clean expired sessions opportunistically
	_, _ = h.db.Exec("DELETE FROM sessions WHERE expires_at <= UTC_TIMESTAMP()")

	h.limiter.Reset(ip)
	database.WriteAuditLog(h.db, user.Username, "login", "Login berhasil")

	c.Cookie(&fiber.Cookie{
		Name:     middleware.SessionCookieName,
		Value:    rawToken,
		Path:     "/",
		Expires:  expiresAt,
		MaxAge:   86400,
		HTTPOnly: true,
		Secure:   h.cfg.CookieSecure,
		SameSite: "Strict",
	})

	return c.JSON(fiber.Map{"user": user})
}

// Logout invalidates the active session token and clears the session cookie.
func (h *AuthHandler) Logout(c fiber.Ctx) error {
	token := strings.TrimSpace(c.Cookies(middleware.SessionCookieName))
	if token != "" {
		_, _ = h.db.Exec("DELETE FROM sessions WHERE token_hash = ?", middleware.HashToken(token))
	}

	c.Cookie(&fiber.Cookie{
		Name:     middleware.SessionCookieName,
		Value:    "",
		Path:     "/",
		Expires:  time.Unix(0, 0),
		MaxAge:   -1,
		HTTPOnly: true,
		Secure:   h.cfg.CookieSecure,
		SameSite: "Strict",
	})

	return c.JSON(fiber.Map{})
}

// Me returns the currently authenticated Operator.
func (h *AuthHandler) Me(c fiber.Ctx) error {
	user := middleware.CurrentUser(c)
	if user == nil {
		return c.Status(fiber.StatusUnauthorized).JSON(fiber.Map{
			"message": "Sesi tidak valid.",
		})
	}
	return c.JSON(fiber.Map{"user": user})
}
