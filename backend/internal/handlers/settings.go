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
	"backend/internal/services"

	"github.com/gofiber/fiber/v3"
)

// SettingsHandler serves device limits, thresholds, and updates settings safely.
type SettingsHandler struct {
	db  *sql.DB
	cfg *config.Config
	tg  *services.TelegramNotifier
}

// NewSettingsHandler creates a new SettingsHandler.
func NewSettingsHandler(db *sql.DB, cfg *config.Config, tg *services.TelegramNotifier) *SettingsHandler {
	return &SettingsHandler{db: db, cfg: cfg, tg: tg}
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
		"temp_min":         36.5,
		"temp_max":         38.5,
		"hum_min":          45,
		"hum_max":          70,
		"lockdown_hum_min": 60,
		"lockdown_hum_max": 78,
		"stale_seconds":    h.cfg.StaleSeconds,
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

func loadNotificationSettings(db *sql.DB, cfg *config.Config, tg *services.TelegramNotifier) models.NotificationSettings {
	res := models.NotificationSettings{
		TelegramEnabled:    true,
		TelegramConfigured: tg != nil && tg.IsConfigured(),
		OverTemperature:    true,
		LowTemperature:     true,
		SensorError:        true,
		HumidityWarning:    true,
		Communication:      true,
		MotorFault:         true,
		General:            true,
	}

	err := db.QueryRow(
		`SELECT telegram_enabled, over_temperature, low_temperature, sensor_error,
		        humidity_warning, communication, motor_fault, general
		 FROM notification_settings WHERE device_id = ? LIMIT 1`,
		cfg.DeviceID,
	).Scan(
		&res.TelegramEnabled,
		&res.OverTemperature,
		&res.LowTemperature,
		&res.SensorError,
		&res.HumidityWarning,
		&res.Communication,
		&res.MotorFault,
		&res.General,
	)
	if err == sql.ErrNoRows {
		_, _ = db.Exec("INSERT IGNORE INTO notification_settings (device_id) VALUES (?)", cfg.DeviceID)
	}
	return res
}

// GetNotificationSettings handles GET /api/settings/notifications (works even when ESP32 is offline).
func (h *SettingsHandler) GetNotificationSettings(c fiber.Ctx) error {
	return c.JSON(loadNotificationSettings(h.db, h.cfg, h.tg))
}

// UpdateNotificationSettings handles PUT /api/settings/notifications (supports partial toggles).
func (h *SettingsHandler) UpdateNotificationSettings(c fiber.Ctx) error {
	user := middleware.CurrentUser(c)

	var body struct {
		TelegramEnabled *bool `json:"telegram_enabled"`
		OverTemperature *bool `json:"over_temperature"`
		LowTemperature  *bool `json:"low_temperature"`
		SensorError     *bool `json:"sensor_error"`
		HumidityWarning *bool `json:"humidity_warning"`
		Communication   *bool `json:"communication"`
		MotorFault      *bool `json:"motor_fault"`
		General         *bool `json:"general"`
	}
	if err := c.Bind().JSON(&body); err != nil {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Format pengaturan notifikasi tidak valid.",
		})
	}

	if body.TelegramEnabled == nil && body.OverTemperature == nil && body.LowTemperature == nil &&
		body.SensorError == nil && body.HumidityWarning == nil && body.Communication == nil &&
		body.MotorFault == nil && body.General == nil {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Minimal satu saklar notifikasi harus disertakan.",
		})
	}

	current := loadNotificationSettings(h.db, h.cfg, h.tg)
	changed := make([]string, 0, 8)
	apply := func(name string, ptr *bool, target *bool) {
		if ptr != nil && *ptr != *target {
			*target = *ptr
			stateStr := "nonaktif"
			if *ptr {
				stateStr = "aktif"
			}
			changed = append(changed, name+"="+stateStr)
		}
	}

	apply("telegram", body.TelegramEnabled, &current.TelegramEnabled)
	apply("over_temperature", body.OverTemperature, &current.OverTemperature)
	apply("low_temperature", body.LowTemperature, &current.LowTemperature)
	apply("sensor_error", body.SensorError, &current.SensorError)
	apply("humidity_warning", body.HumidityWarning, &current.HumidityWarning)
	apply("communication", body.Communication, &current.Communication)
	apply("motor_fault", body.MotorFault, &current.MotorFault)
	apply("general", body.General, &current.General)

	if _, err := h.db.Exec(
		`INSERT INTO notification_settings (
			device_id, telegram_enabled, over_temperature, low_temperature, sensor_error,
			humidity_warning, communication, motor_fault, general, updated_at
		) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, UTC_TIMESTAMP())
		ON DUPLICATE KEY UPDATE
			telegram_enabled = VALUES(telegram_enabled),
			over_temperature = VALUES(over_temperature),
			low_temperature = VALUES(low_temperature),
			sensor_error = VALUES(sensor_error),
			humidity_warning = VALUES(humidity_warning),
			communication = VALUES(communication),
			motor_fault = VALUES(motor_fault),
			general = VALUES(general),
			updated_at = UTC_TIMESTAMP()`,
		h.cfg.DeviceID,
		current.TelegramEnabled, current.OverTemperature, current.LowTemperature, current.SensorError,
		current.HumidityWarning, current.Communication, current.MotorFault, current.General,
	); err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal menyimpan pengaturan notifikasi.",
		})
	}

	if len(changed) > 0 {
		database.WriteAuditLog(h.db, user.Username, "config_change",
			"Pengaturan notifikasi diperbarui: "+strings.Join(changed, ", "))
	}
	return c.JSON(current)
}

// TestTelegramNotification handles POST /api/settings/notifications/test-telegram.
func (h *SettingsHandler) TestTelegramNotification(c fiber.Ctx) error {
	user := middleware.CurrentUser(c)
	if h.tg == nil || !h.tg.IsConfigured() {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Token atau Chat ID Telegram belum diisi di file .env server.",
		})
	}
	if err := h.tg.SendTest(user.Username); err != nil {
		return c.Status(fiber.StatusBadGateway).JSON(fiber.Map{
			"message": err.Error(),
		})
	}
	return c.JSON(fiber.Map{
		"status":  "ok",
		"message": "Pesan uji berhasil dikirim ke Telegram Bot.",
	})
}

// TestFCMNotification handles POST /api/settings/notifications/test-fcm.
func (h *SettingsHandler) TestFCMNotification(c fiber.Ctx) error {
	user := middleware.CurrentUser(c)
	if h.tg == nil || !h.tg.HasFCM() {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Firebase Cloud Messaging belum dikonfigurasi di server.",
		})
	}
	if err := h.tg.SendTestFCM(user.Username); err != nil {
		return c.Status(fiber.StatusBadGateway).JSON(fiber.Map{
			"message": err.Error(),
		})
	}
	return c.JSON(fiber.Map{
		"status":  "ok",
		"message": "Push notification uji berhasil dikirim ke Firebase Cloud Messaging.",
	})
}
