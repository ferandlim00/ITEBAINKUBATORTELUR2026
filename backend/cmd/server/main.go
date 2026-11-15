package main

import (
	"context"
	"log"
	"os"
	"os/signal"
	"path/filepath"
	"strings"
	"syscall"
	"time"

	"backend/internal/config"
	"backend/internal/database"
	"backend/internal/handlers"
	"backend/internal/middleware"
	"backend/internal/services"

	"github.com/gofiber/fiber/v3"
	"github.com/gofiber/fiber/v3/middleware/cors"
	"github.com/gofiber/fiber/v3/middleware/logger"
	recovermw "github.com/gofiber/fiber/v3/middleware/recover"
)

func main() {
	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("Gagal memuat konfigurasi: %v", err)
	}

	db, err := database.Connect(cfg)
	if err != nil {
		log.Fatalf("Gagal menginisialisasi database MySQL: %v", err)
	}
	defer db.Close()

	tg := services.NewTelegramNotifier(cfg.TelegramBotToken, cfg.TelegramChatID)
	limiter := middleware.NewLoginRateLimiter(5, time.Minute)

	authHandler := handlers.NewAuthHandler(db, cfg, limiter)
	telemetryHandler := handlers.NewTelemetryHandler(db, cfg, tg)
	commandsHandler := handlers.NewCommandsHandler(db, cfg)
	settingsHandler := handlers.NewSettingsHandler(db, cfg)
	alertsAuditHandler := handlers.NewAlertsAuditHandler(db, cfg)

	app := fiber.New(fiber.Config{
		AppName:     "Smart Hatchery API v2.1",
		ProxyHeader: fiber.HeaderXForwardedFor,
		TrustProxy:  true,
		TrustProxyConfig: fiber.TrustProxyConfig{
			Loopback:  true,
			Private:   true,
			LinkLocal: true,
		},
	})

	warnWeakProductionDefaults(cfg)

	app.Use(recovermw.New())
	app.Use(logger.New())

	origins := splitOrigins(cfg.CORSAllowOrigins)
	app.Use(cors.New(cors.Config{
		AllowOriginsFunc: func(origin string) bool {
			return isAllowedOrigin(origin, origins)
		},
		AllowMethods:     []string{"GET", "POST", "PUT", "OPTIONS"},
		AllowHeaders:     []string{"Origin", "Content-Type", "Accept", "X-Device-Key"},
		AllowCredentials: true,
	}))

	api := app.Group("/api")

	// Health check publik
	api.Get("/health", func(c fiber.Ctx) error {
		if err := db.Ping(); err != nil {
			return c.Status(fiber.StatusServiceUnavailable).JSON(fiber.Map{
				"status":  "error",
				"message": "Database tidak merespons.",
			})
		}
		return c.JSON(fiber.Map{
			"status":      "ok",
			"server_time": time.Now().UTC().Format(time.RFC3339),
		})
	})

	// Endpoint Autentikasi Publik (POST /api/auth/login dilindungi rate-limiter 5x/menit)
	api.Post("/auth/login", authHandler.Login)
	api.Post("/auth/logout", authHandler.Logout)

	// Endpoint Khusus Perangkat ESP32 (dilindungi header X-Device-Key)
	api.Post("/device/telemetry", middleware.RequireDeviceKey(cfg.DeviceAPIKey), telemetryHandler.IngestDeviceTelemetry)

	// Seluruh endpoint di bawah ini wajib terautentikasi Operator via HttpOnly Session Cookie
	protected := api.Group("", middleware.RequireAuth(db))
	protected.Get("/auth/me", authHandler.Me)

	protected.Get("/telemetry/latest", telemetryHandler.GetLatest)
	protected.Get("/telemetry/history", telemetryHandler.GetHistory)

	protected.Get("/alerts", alertsAuditHandler.GetAlerts)
	protected.Get("/audit-logs", alertsAuditHandler.GetAuditLogs)

	protected.Post("/commands", commandsHandler.CreateCommand)

	protected.Get("/settings/device", settingsHandler.GetDeviceSettings)
	protected.Get("/settings/limits", settingsHandler.GetLimits)
	protected.Get("/settings/thresholds", settingsHandler.GetThresholds)
	protected.Put("/settings/target-temperature", settingsHandler.UpdateTargetTemperature)
	protected.Put("/settings/motor", settingsHandler.UpdateMotorSettings)
	protected.Put("/settings/wifi", settingsHandler.UpdateWifiSettings)

	// Sajikan build statis Frontend SvelteKit (SPA) jika folder public/index.html tersedia (mode Railway / Single Container)
	registerSPAFallback(app, cfg.StaticDir)

	// Background watcher tiap 5 detik untuk mendeteksi perangkat offline > stale_seconds (15 detik)
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	go func() {
		ticker := time.NewTicker(5 * time.Second)
		defer ticker.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
				handlers.CheckStaleDevice(db, cfg)
			}
		}
	}()

	// Graceful shutdown on Ctrl+C / SIGTERM
	go func() {
		sigCh := make(chan os.Signal, 1)
		signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
		<-sigCh
		cancel()
		_ = app.Shutdown()
	}()

	addr := ":" + cfg.AppPort
	log.Printf("Smart Hatchery Backend Go Fiber berjalan di http://localhost%s", addr)
	if err := app.Listen(addr); err != nil {
		log.Fatalf("Server berhenti dengan error: %v", err)
	}
}

func splitOrigins(raw string) []string {
	parts := strings.Split(raw, ",")
	out := make([]string, 0, len(parts))
	for _, p := range parts {
		if trimmed := strings.TrimSpace(p); trimmed != "" {
			out = append(out, trimmed)
		}
	}
	if len(out) == 0 {
		return []string{"http://localhost:5173"}
	}
	return out
}

func isAllowedOrigin(origin string, allowed []string) bool {
	o := strings.TrimSpace(strings.ToLower(origin))
	if o == "" {
		return true
	}
	for _, a := range allowed {
		if strings.EqualFold(o, strings.TrimSpace(a)) || a == "*" {
			return true
		}
	}
	// Izinkan domain bawaan Railway, custom domain .cc.cd, serta IP (100.x.x.x) & domain Tailscale (.ts.net)
	if strings.HasSuffix(o, ".up.railway.app") ||
		strings.HasSuffix(o, "itebaincubatortelur.cc.cd") ||
		strings.HasSuffix(o, ".ts.net") ||
		strings.HasPrefix(o, "http://100.") ||
		strings.HasPrefix(o, "https://100.") {
		return true
	}
	return false
}

func registerSPAFallback(app *fiber.App, staticDir string) {
	indexFile := filepath.Join(staticDir, "index.html")
	info, err := os.Stat(indexFile)
	if err != nil || info.IsDir() {
		return
	}
	absRoot, err := filepath.Abs(staticDir)
	if err != nil {
		return
	}
	log.Printf("Menyajikan Frontend SPA dari direktori statis: %s", absRoot)

	app.Use(func(c fiber.Ctx) error {
		path := c.Path()
		if path == "/api" || strings.HasPrefix(path, "/api/") {
			return c.Status(fiber.StatusNotFound).JSON(fiber.Map{
				"message": "Endpoint API tidak ditemukan.",
			})
		}
		if c.Method() != fiber.MethodGet && c.Method() != fiber.MethodHead {
			return c.Next()
		}

		cleanRel := filepath.Clean(strings.TrimPrefix(path, "/"))
		if cleanRel != "." && !strings.HasPrefix(cleanRel, "..") {
			candidate := filepath.Join(absRoot, cleanRel)
			if st, err := os.Stat(candidate); err == nil && !st.IsDir() {
				return c.SendFile(candidate)
			}
		}

		c.Set("Cache-Control", "no-cache, no-store, must-revalidate")
		return c.SendFile(filepath.Join(absRoot, "index.html"))
	})
}

func warnWeakProductionDefaults(cfg *config.Config) {
	if !strings.EqualFold(cfg.AppEnv, "production") {
		return
	}
	if cfg.DeviceAPIKey == "secret-device-token-esp32" {
		log.Println("PERINGATAN KEAMANAN: DEVICE_API_KEY masih menggunakan nilai bawaan dev. Ganti dengan token acak untuk VPS produksi!")
	}
	if !cfg.CookieSecure {
		log.Println("PERINGATAN KEAMANAN: COOKIE_SECURE=false di mode production. Aktifkan COOKIE_SECURE=true jika sudah memakai HTTPS/domain.")
	}
}
