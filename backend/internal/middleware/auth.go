package middleware

import (
	"crypto/sha256"
	"database/sql"
	"encoding/hex"
	"strings"

	"backend/internal/database"
	"backend/internal/models"

	"github.com/gofiber/fiber/v3"
)

const SessionCookieName = "session_id"

// HashToken produces a hex-encoded SHA-256 digest of a raw session token.
func HashToken(rawToken string) string {
	sum := sha256.Sum256([]byte(rawToken))
	return hex.EncodeToString(sum[:])
}

// RequireAuth validates the HttpOnly session cookie against MySQL sessions.
func RequireAuth(db *sql.DB) fiber.Handler {
	return func(c fiber.Ctx) error {
		token := strings.TrimSpace(c.Cookies(SessionCookieName))
		if token == "" {
			if c.Path() != "/api/auth/me" {
				database.WriteAuditLog(db, "anonim", "access_denied", "Akses ditolak: sesi tidak ditemukan pada "+c.Path())
			}
			return c.Status(fiber.StatusUnauthorized).JSON(fiber.Map{
				"message": "Sesi tidak valid atau telah berakhir. Silakan masuk kembali.",
			})
		}

		tokenHash := HashToken(token)
		var user models.User
		err := db.QueryRow(
			`SELECT u.id, u.username, u.role
			 FROM sessions s
			 JOIN users u ON u.id = s.user_id
			 WHERE s.token_hash = ? AND s.expires_at > UTC_TIMESTAMP()
			 LIMIT 1`,
			tokenHash,
		).Scan(&user.ID, &user.Username, &user.Role)

		if err != nil {
			database.WriteAuditLog(db, "anonim", "access_denied", "Akses ditolak: sesi kedaluwarsa atau tidak valid pada "+c.Path())
			return c.Status(fiber.StatusUnauthorized).JSON(fiber.Map{
				"message": "Sesi tidak valid atau telah berakhir. Silakan masuk kembali.",
			})
		}

		c.SetLocal("user", &user)
		return c.Next()
	}
}

// CurrentUser extracts the authenticated user from Fiber locals.
func CurrentUser(c fiber.Ctx) *models.User {
	u, _ := c.Value("user").(*models.User)
	return u
}
