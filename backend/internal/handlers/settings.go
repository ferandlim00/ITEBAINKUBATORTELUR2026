package handlers

import (
	"database/sql"
	"fmt"
	"strings"

	"backend/internal/config"
	"backend/internal/crypto"
	"backend/internal/database"
	"backend/internal/middleware"
	"backend/internal/models"

	"github.com/gofiber/fiber/v3"
)

// SettingsHandler serves device limits, thresholds, and updates settings safely.
type SettingsHandler struct {
	db  *sql.DB
	cfg *config.Config
}

// NewSettingsHandler creates a new SettingsHandler.
func NewSettingsHandler(db *sql.DB, cfg *config.Config) *SettingsHandler {
	return &SettingsHandler{db: db, cfg: cfg}
}

// GetDeviceSettings handles GET /api/settings/device (never exposes Wi-Fi password).
func (h *SettingsHandler) GetDeviceSettings(c fiber.Ctx) error {
	var resp models.DeviceSettingsResponse
	err := h.db.QueryRow(
		`SELECT target_temperature, motor_interval_minutes, motor_duration_seconds, motor_speed_percent, wifi_ssid
		 FROM device_settings WHERE device_id = ? LIMIT 1`,
		h.cfg.DeviceID,
	).Scan(
		&resp.TargetTemperature,
		&resp.Motor.IntervalMinutes,
		&resp.Motor.DurationSeconds,
		&resp.Motor.SpeedPercent,
		&resp.Wifi.SSID,
	)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal membaca pengaturan perangkat.",
		})
	}
	return c.JSON(resp)
}

// GetLimits handles GET /api/settings/limits.
func (h *SettingsHandler) GetLimits(c fiber.Ctx) error {
	return c.JSON(fiber.Map{
		"target_temperature":    fiber.Map{"min": 36.5, "max": 38.5},
		"lamp_manual_intensity": fiber.Map{"min": 0, "max": 80},
		"ventilation_angle":     fiber.Map{"min": 0, "max": 90},
		"motor": fiber.Map{
			"interval_minutes": fiber.Map{"min": 30, "max": 480},
			"duration_seconds": fiber.Map{"min": 5, "max": 60},
			"speed_percent":    fiber.Map{"min": 10, "max": 70},
		},
	})
}

// GetThresholds handles GET /api/settings/thresholds (read-only safety bounds).
func (h *SettingsHandler) GetThresholds(c fiber.Ctx) error {
	return c.JSON(fiber.Map{
		"temp_min":      36.5,
		"temp_max":      38.5,
		"hum_min":       45,
		"hum_max":       70,
		"stale_seconds": h.cfg.StaleSeconds,
	})
}

// UpdateTargetTemperature handles PUT /api/settings/target-temperature.
func (h *SettingsHandler) UpdateTargetTemperature(c fiber.Ctx) error {
	user := middleware.CurrentUser(c)
	snap, isStale, err := GetDeviceSnapshot(h.db, h.cfg)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal memeriksa status perangkat.",
		})
	}
	if isStale || snap.Connection == "offline" {
		return c.Status(fiber.StatusConflict).JSON(fiber.Map{
			"message": "Perangkat sedang offline atau data kedaluwarsa. Pengaturan ditolak.",
		})
	}

	var body struct {
		Value *float64 `json:"value"`
	}
	if err := c.Bind().JSON(&body); err != nil || body.Value == nil {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Nilai target suhu wajib diisi.",
		})
	}

	val := *body.Value
	if val < 36.5 || val > 38.5 {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Target suhu harus berada pada rentang 36.5°C sampai 38.5°C.",
		})
	}

	if _, err := h.db.Exec(
		"UPDATE device_settings SET target_temperature = ?, updated_at = UTC_TIMESTAMP() WHERE device_id = ?",
		val, h.cfg.DeviceID,
	); err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal menyimpan target suhu.",
		})
	}

	database.WriteAuditLog(h.db, user.Username, "config_change", fmt.Sprintf("Target suhu diubah ke %.1f°C", val))
	return c.JSON(fiber.Map{"status": "sent"})
}

// UpdateMotorSettings handles PUT /api/settings/motor (supports full or partial fields).
func (h *SettingsHandler) UpdateMotorSettings(c fiber.Ctx) error {
	user := middleware.CurrentUser(c)
	snap, isStale, err := GetDeviceSnapshot(h.db, h.cfg)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal memeriksa status perangkat.",
		})
	}
	if isStale || snap.Connection == "offline" {
		return c.Status(fiber.StatusConflict).JSON(fiber.Map{
			"message": "Perangkat sedang offline atau data kedaluwarsa. Pengaturan ditolak.",
		})
	}

	var body struct {
		IntervalMinutes *int `json:"interval_minutes"`
		DurationSeconds *int `json:"duration_seconds"`
		SpeedPercent    *int `json:"speed_percent"`
	}
	if err := c.Bind().JSON(&body); err != nil {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Format pengaturan motor tidak valid.",
		})
	}

	if body.IntervalMinutes == nil && body.DurationSeconds == nil && body.SpeedPercent == nil {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Minimal satu parameter motor harus diisi.",
		})
	}

	var current models.MotorSettings
	if err := h.db.QueryRow(
		"SELECT motor_interval_minutes, motor_duration_seconds, motor_speed_percent FROM device_settings WHERE device_id = ? LIMIT 1",
		h.cfg.DeviceID,
	).Scan(&current.IntervalMinutes, &current.DurationSeconds, &current.SpeedPercent); err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal membaca pengaturan motor saat ini.",
		})
	}

	if body.IntervalMinutes != nil {
		if *body.IntervalMinutes < 30 || *body.IntervalMinutes > 480 {
			return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
				"message": "Interval pembalikan motor harus di antara 30 sampai 480 menit.",
			})
		}
		current.IntervalMinutes = *body.IntervalMinutes
	}
	if body.DurationSeconds != nil {
		if *body.DurationSeconds < 5 || *body.DurationSeconds > 60 {
			return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
				"message": "Durasi putar motor harus di antara 5 sampai 60 detik.",
			})
		}
		current.DurationSeconds = *body.DurationSeconds
	}
	if body.SpeedPercent != nil {
		if *body.SpeedPercent < 10 || *body.SpeedPercent > 70 {
			return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
				"message": "Kecepatan motor harus di antara 10% sampai 70%.",
			})
		}
		current.SpeedPercent = *body.SpeedPercent
	}

	if _, err := h.db.Exec(
		`UPDATE device_settings
		 SET motor_interval_minutes = ?, motor_duration_seconds = ?, motor_speed_percent = ?, updated_at = UTC_TIMESTAMP()
		 WHERE device_id = ?`,
		current.IntervalMinutes, current.DurationSeconds, current.SpeedPercent, h.cfg.DeviceID,
	); err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal menyimpan pengaturan motor.",
		})
	}

	database.WriteAuditLog(h.db, user.Username, "config_change",
		fmt.Sprintf("Parameter motor diubah (interval=%dm, durasi=%ds, kecepatan=%d%%)",
			current.IntervalMinutes, current.DurationSeconds, current.SpeedPercent))
	return c.JSON(fiber.Map{"status": "sent"})
}

// UpdateWifiSettings handles PUT /api/settings/wifi, encrypting password with AES-256-GCM.
func (h *SettingsHandler) UpdateWifiSettings(c fiber.Ctx) error {
	user := middleware.CurrentUser(c)
	snap, isStale, err := GetDeviceSnapshot(h.db, h.cfg)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal memeriksa status perangkat.",
		})
	}
	if isStale || snap.Connection == "offline" {
		return c.Status(fiber.StatusConflict).JSON(fiber.Map{
			"message": "Perangkat sedang offline atau data kedaluwarsa. Pengaturan Wi-Fi ditolak.",
		})
	}

	var body struct {
		SSID     string `json:"ssid"`
		Password string `json:"password"`
	}
	if err := c.Bind().JSON(&body); err != nil {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Format konfigurasi Wi-Fi tidak valid.",
		})
	}

	ssid := strings.TrimSpace(body.SSID)
	if len(ssid) < 1 || len(ssid) > 32 {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Nama Wi-Fi (SSID) wajib 1 sampai 32 karakter.",
		})
	}
	if len(body.Password) < 8 || len(body.Password) > 63 {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Kata sandi Wi-Fi wajib 8 sampai 63 karakter.",
		})
	}

	encPass, nonce, err := crypto.EncryptAESGCM(h.cfg.EncryptionKey, body.Password)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal mengenkripsi kredensial Wi-Fi.",
		})
	}

	if _, err := h.db.Exec(
		`UPDATE device_settings
		 SET wifi_ssid = ?, wifi_password_enc = ?, wifi_password_nonce = ?, wifi_sync_pending = 1, updated_at = UTC_TIMESTAMP()
		 WHERE device_id = ?`,
		ssid, encPass, nonce, h.cfg.DeviceID,
	); err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal menyimpan konfigurasi Wi-Fi.",
		})
	}

	// Kata sandi Wi-Fi dilarang keras dicatat ke dalam audit log (PRD FUN 07 & SEC 03)
	database.WriteAuditLog(h.db, user.Username, "config_change", "Konfigurasi Wi-Fi diubah ke SSID: "+ssid)
	return c.JSON(fiber.Map{"status": "sent"})
}
