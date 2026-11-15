package config

import (
	"crypto/sha256"
	"encoding/hex"
	"os"
	"strconv"
	"strings"

	"github.com/joho/godotenv"
)

// Config holds all runtime configuration loaded from environment variables.
type Config struct {
	AppPort          string
	AppEnv           string
	CookieSecure     bool
	CORSAllowOrigins string
	StaticDir        string

	DBHost     string
	DBPort     string
	DBUser     string
	DBPassword string
	DBName     string

	EncryptionKey []byte
	DeviceID      string
	DeviceAPIKey  string

	OperatorUsername string
	OperatorPassword string

	TelegramBotToken string
	TelegramChatID   string

	StaleSeconds int
}

// Load reads .env (if present) and environment variables into Config.
func Load() (*Config, error) {
	_ = godotenv.Load()

	stale := 15
	if v := strings.TrimSpace(os.Getenv("STALE_SECONDS")); v != "" {
		if n, err := strconv.Atoi(v); err == nil && n > 0 {
			stale = n
		}
	}

	rawKey := getEnv("APP_ENCRYPTION_KEY", "dev-only-secret-key-change-in-production")
	encKey := parseEncryptionKey(rawKey)

	return &Config{
		AppPort:          getEnvFirst([]string{"PORT", "APP_PORT"}, "3000"),
		AppEnv:           getEnv("APP_ENV", "development"),
		CookieSecure:     strings.EqualFold(getEnv("COOKIE_SECURE", "false"), "true"),
		CORSAllowOrigins: getEnv("CORS_ALLOW_ORIGINS", "http://localhost:5173,http://localhost:3000,https://itebaincubatortelur.cc.cd"),
		StaticDir:        getEnv("STATIC_DIR", "./public"),

		DBHost:     getEnvFirst([]string{"DB_HOST", "MYSQLHOST"}, "127.0.0.1"),
		DBPort:     getEnvFirst([]string{"DB_PORT", "MYSQLPORT"}, "3306"),
		DBUser:     getEnvFirst([]string{"DB_USER", "MYSQLUSER"}, "root"),
		DBPassword: getEnvFirst([]string{"DB_PASSWORD", "MYSQLPASSWORD", "MYSQL_ROOT_PASSWORD"}, ""),
		DBName:     getEnvFirst([]string{"DB_NAME", "MYSQLDATABASE", "MYSQL_DATABASE"}, "smart_hatchery"),

		EncryptionKey: encKey,
		DeviceID:      getEnv("DEVICE_ID", "inkubator-01"),
		DeviceAPIKey:  getEnv("DEVICE_API_KEY", "secret-device-token-esp32"),

		OperatorUsername: getEnv("OPERATOR_USERNAME", "operator1"),
		OperatorPassword: getEnv("OPERATOR_PASSWORD", "operator12345"),

		TelegramBotToken: strings.TrimSpace(os.Getenv("TELEGRAM_BOT_TOKEN")),
		TelegramChatID:   strings.TrimSpace(os.Getenv("TELEGRAM_CHAT_ID")),

		StaleSeconds: stale,
	}, nil
}

func getEnv(key, fallback string) string {
	if v := strings.TrimSpace(os.Getenv(key)); v != "" {
		return v
	}
	return fallback
}

func getEnvFirst(keys []string, fallback string) string {
	for _, k := range keys {
		if v := strings.TrimSpace(os.Getenv(k)); v != "" {
			return v
		}
	}
	return fallback
}

// parseEncryptionKey supports a 64-char hex string (32 bytes) or hashes arbitrary secrets via SHA-256.
func parseEncryptionKey(raw string) []byte {
	trimmed := strings.TrimSpace(raw)
	if len(trimmed) == 64 {
		if b, err := hex.DecodeString(trimmed); err == nil && len(b) == 32 {
			return b
		}
	}
	sum := sha256.Sum256([]byte(trimmed))
	return sum[:]
}
