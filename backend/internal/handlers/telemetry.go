package handlers

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"math"
	"strconv"
	"strings"
	"time"

	"backend/internal/config"
	"backend/internal/crypto"
	"backend/internal/models"
	"backend/internal/services"

	"github.com/gofiber/fiber/v3"
)

// TelemetryHandler serves telemetry queries and handles ESP32 telemetry ingestion.
type TelemetryHandler struct {
	db  *sql.DB
	cfg *config.Config
	tg  *services.TelegramNotifier
}

// NewTelemetryHandler creates a new TelemetryHandler.
func NewTelemetryHandler(db *sql.DB, cfg *config.Config, tg *services.TelegramNotifier) *TelemetryHandler {
	return &TelemetryHandler{db: db, cfg: cfg, tg: tg}
}

// GetLatest handles GET /api/telemetry/latest.
func (h *TelemetryHandler) GetLatest(c fiber.Ctx) error {
	snap, _, err := GetDeviceSnapshot(h.db, h.cfg)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal mengambil data telemetri terbaru.",
		})
	}
	return c.JSON(snap)
}

// GetHistory handles GET /api/telemetry/history?from=&to=&max_points=.
func (h *TelemetryHandler) GetHistory(c fiber.Ctx) error {
	now := time.Now().UTC()
	fromTime := now.Add(-6 * time.Hour)
	toTime := now

	if v := strings.TrimSpace(c.Query("from")); v != "" {
		t, err := parseFlexibleISO(v)
		if err != nil {
			return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
				"message": "Parameter waktu 'from' tidak valid.",
			})
		}
		fromTime = t.UTC()
	}

	if v := strings.TrimSpace(c.Query("to")); v != "" {
		t, err := parseFlexibleISO(v)
		if err != nil {
			return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
				"message": "Parameter waktu 'to' tidak valid.",
			})
		}
		toTime = t.UTC()
	}

	maxPoints := 120
	if mp, err := strconv.Atoi(c.Query("max_points", "120")); err == nil && mp > 0 {
		if mp < 10 {
			mp = 10
		} else if mp > 500 {
			mp = 500
		}
		maxPoints = mp
	}

	rows, err := h.db.Query(
		`SELECT recorded_at, temperature, humidity, lamp_intensity
		 FROM telemetry_history
		 WHERE device_id = ? AND recorded_at >= ? AND recorded_at <= ?
		 ORDER BY recorded_at ASC`,
		h.cfg.DeviceID, fromTime, toTime,
	)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal mengambil riwayat grafik sensor.",
		})
	}
	defer rows.Close()

	type rawPoint struct {
		t    time.Time
		temp float64
		hum  float64
		lamp int
	}
	raw := make([]rawPoint, 0)
	for rows.Next() {
		var p rawPoint
		if err := rows.Scan(&p.t, &p.temp, &p.hum, &p.lamp); err != nil {
			return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
				"message": "Gagal membaca riwayat grafik sensor.",
			})
		}
		raw = append(raw, p)
	}

	if len(raw) == 0 {
		return c.JSON([]models.HistoryPoint{})
	}

	// Downsample / average into at most maxPoints buckets when rows exceed maxPoints
	if len(raw) <= maxPoints {
		out := make([]models.HistoryPoint, 0, len(raw))
		for _, r := range raw {
			out = append(out, models.HistoryPoint{
				T:             r.t.UTC().Format(time.RFC3339),
				Temperature:   math.Round(r.temp*100) / 100,
				Humidity:      math.Round(r.hum*10) / 10,
				LampIntensity: r.lamp,
			})
		}
		return c.JSON(out)
	}

	out := make([]models.HistoryPoint, 0, maxPoints)
	bucketSize := float64(len(raw)) / float64(maxPoints)
	for i := 0; i < maxPoints; i++ {
		start := int(math.Floor(float64(i) * bucketSize))
		end := int(math.Floor(float64(i+1) * bucketSize))
		if end <= start {
			end = start + 1
		}
		if end > len(raw) {
			end = len(raw)
		}
		if start >= len(raw) {
			break
		}

		var sumTemp, sumHum float64
		var sumLamp int
		var sumUnix int64
		count := end - start
		for j := start; j < end; j++ {
			sumTemp += raw[j].temp
			sumHum += raw[j].hum
			sumLamp += raw[j].lamp
			sumUnix += raw[j].t.UnixMilli()
		}

		avgT := time.UnixMilli(sumUnix / int64(count)).UTC()
		out = append(out, models.HistoryPoint{
			T:             avgT.Format(time.RFC3339),
			Temperature:   math.Round((sumTemp/float64(count))*100) / 100,
			Humidity:      math.Round((sumHum/float64(count))*10) / 10,
			LampIntensity: int(math.Round(float64(sumLamp) / float64(count))),
		})
	}

	return c.JSON(out)
}

// IngestDeviceTelemetry handles POST /api/device/telemetry from ESP32 every 5 seconds.
func (h *TelemetryHandler) IngestDeviceTelemetry(c fiber.Ctx) error {
	var p models.DeviceTelemetryPayload
	if err := c.Bind().JSON(&p); err != nil {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Payload telemetri tidak valid.",
		})
	}

	deviceID := strings.TrimSpace(p.DeviceID)
	if deviceID == "" {
		deviceID = h.cfg.DeviceID
	}

	normalizeIncubationStage(&p)

	now := time.Now().UTC()
	deviceUpdated := now
	if p.UpdatedAt != "" {
		if parsed, err := parseFlexibleISO(p.UpdatedAt); err == nil {
			deviceUpdated = parsed.UTC()
		}
	}

	var incDay, incTotal sql.NullInt64
	if p.Incubation.CurrentDay != nil {
		incDay = sql.NullInt64{Int64: int64(*p.Incubation.CurrentDay), Valid: true}
	}
	if p.Incubation.TotalDays != nil {
		incTotal = sql.NullInt64{Int64: int64(*p.Incubation.TotalDays), Valid: true}
	}
	incStarted := parseNullableTime(p.Incubation.StartedAt)
	incFinish := parseNullableTime(p.Incubation.FinishAt)
	incStage := toNullString(p.Incubation.Stage)

	lastTurned := parseNullableTime(p.Turning.LastTurnedAt)
	nextTurning := parseNullableTime(p.Turning.NextTurningAt)

	var wifiRSSI sql.NullInt64
	if p.Wifi.RSSI != nil {
		wifiRSSI = sql.NullInt64{Int64: int64(*p.Wifi.RSSI), Valid: true}
	}

	var lastCmdID sql.NullInt64
	var lastCmdTarget, lastCmdAction, lastCmdStatus sql.NullString
	var lastCmdUpdated sql.NullTime
	if p.LastCommand != nil && p.LastCommand.ID > 0 {
		lastCmdID = sql.NullInt64{Int64: p.LastCommand.ID, Valid: true}
		lastCmdTarget = sql.NullString{String: p.LastCommand.Target, Valid: p.LastCommand.Target != ""}
		lastCmdAction = sql.NullString{String: p.LastCommand.Action, Valid: p.LastCommand.Action != ""}
		lastCmdStatus = sql.NullString{String: p.LastCommand.Status, Valid: p.LastCommand.Status != ""}
		if t, err := parseFlexibleISO(p.LastCommand.UpdatedAt); err == nil {
			lastCmdUpdated = sql.NullTime{Time: t.UTC(), Valid: true}
		} else {
			lastCmdUpdated = sql.NullTime{Time: now, Valid: true}
		}

		if p.LastCommand.Status == "executed" || p.LastCommand.Status == "failed" {
			_, _ = h.db.Exec(
				"UPDATE commands SET status = ?, updated_at = UTC_TIMESTAMP() WHERE id = ? AND device_id = ? AND status = 'dispatched'",
				p.LastCommand.Status, p.LastCommand.ID, deviceID,
			)
		}
	}

	var prevIncStatus, prevIncStage sql.NullString
	hasPrevState := h.db.QueryRow(
		"SELECT incubation_status, incubation_stage FROM device_state WHERE device_id = ? LIMIT 1",
		deviceID,
	).Scan(&prevIncStatus, &prevIncStage) == nil

	_, err := h.db.Exec(
		`INSERT INTO device_state (
			device_id, last_telemetry_at, device_updated_at,
			temperature, target_temperature, humidity,
			incubation_status, incubation_current_day, incubation_total_days,
			incubation_started_at, incubation_finish_at, incubation_stage,
			system_health, uptime_seconds, summary,
			lamp_mode, lamp_intensity, lamp_fuzzy_active,
			vent_mode, vent_angle, vent_fuzzy_active,
			motor_running, motor_encoder_ok, turning_status,
			last_turned_at, next_turning_at,
			wifi_connected, wifi_ssid, wifi_rssi,
			last_command_id, last_command_target, last_command_action,
			last_command_status, last_command_updated_at
		) VALUES (
			?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?
		)
		ON DUPLICATE KEY UPDATE
			last_telemetry_at = VALUES(last_telemetry_at),
			device_updated_at = VALUES(device_updated_at),
			temperature = VALUES(temperature),
			target_temperature = VALUES(target_temperature),
			humidity = VALUES(humidity),
			incubation_status = VALUES(incubation_status),
			incubation_current_day = VALUES(incubation_current_day),
			incubation_total_days = VALUES(incubation_total_days),
			incubation_started_at = VALUES(incubation_started_at),
			incubation_finish_at = VALUES(incubation_finish_at),
			incubation_stage = VALUES(incubation_stage),
			system_health = VALUES(system_health),
			uptime_seconds = VALUES(uptime_seconds),
			summary = VALUES(summary),
			lamp_mode = VALUES(lamp_mode),
			lamp_intensity = VALUES(lamp_intensity),
			lamp_fuzzy_active = VALUES(lamp_fuzzy_active),
			vent_mode = VALUES(vent_mode),
			vent_angle = VALUES(vent_angle),
			vent_fuzzy_active = VALUES(vent_fuzzy_active),
			motor_running = VALUES(motor_running),
			motor_encoder_ok = VALUES(motor_encoder_ok),
			turning_status = VALUES(turning_status),
			last_turned_at = VALUES(last_turned_at),
			next_turning_at = VALUES(next_turning_at),
			wifi_connected = VALUES(wifi_connected),
			wifi_ssid = VALUES(wifi_ssid),
			wifi_rssi = VALUES(wifi_rssi),
			last_command_id = COALESCE(VALUES(last_command_id), last_command_id),
			last_command_target = COALESCE(VALUES(last_command_target), last_command_target),
			last_command_action = COALESCE(VALUES(last_command_action), last_command_action),
			last_command_status = COALESCE(VALUES(last_command_status), last_command_status),
			last_command_updated_at = COALESCE(VALUES(last_command_updated_at), last_command_updated_at)`,
		deviceID, now, deviceUpdated,
		p.Temperature, p.TargetTemperature, p.Humidity,
		defaultIfEmpty(p.Incubation.Status, "idle"), incDay, incTotal,
		incStarted, incFinish, incStage,
		defaultIfEmpty(p.DeviceCondition.SystemHealth, "normal"), p.DeviceCondition.UptimeSeconds, p.DeviceCondition.Summary,
		defaultIfEmpty(p.Lamp.Mode, "auto"), p.Lamp.Intensity, p.Lamp.FuzzyActive,
		defaultIfEmpty(p.Ventilation.Mode, "auto"), p.Ventilation.Angle, p.Ventilation.FuzzyActive,
		p.Motor.Running, p.Motor.EncoderOK, defaultIfEmpty(p.Turning.Status, "idle"),
		lastTurned, nextTurning,
		p.Wifi.Connected, p.Wifi.SSID, wifiRSSI,
		lastCmdID, lastCmdTarget, lastCmdAction, lastCmdStatus, lastCmdUpdated,
	)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal menyimpan data telemetri.",
		})
	}

	_, _ = h.db.Exec(
		`INSERT INTO telemetry_history (device_id, temperature, humidity, lamp_intensity, vent_angle, recorded_at)
		 VALUES (?, ?, ?, ?, ?, ?)`,
		deviceID, p.Temperature, p.Humidity, p.Lamp.Intensity, p.Ventilation.Angle, now,
	)

	if p.Wifi.Connected {
		if ssid := strings.TrimSpace(p.Wifi.SSID); ssid != "" {
			_, _ = h.db.Exec(
				`UPDATE device_settings
				 SET wifi_ssid = ?
				 WHERE device_id = ? AND wifi_sync_pending = 0 AND wifi_ssid <> ?`,
				ssid, deviceID, ssid,
			)
		}
	}

	notifCfg := loadNotificationSettings(h.db, h.cfg, h.tg)
	h.reconcileAlerts(deviceID, p, notifCfg)

	if hasPrevState && notifCfg.General && h.tg != nil {
		currStatus := defaultIfEmpty(p.Incubation.Status, "idle")
		currStage := ""
		if p.Incubation.Stage != nil {
			currStage = strings.TrimSpace(*p.Incubation.Stage)
		}
		if prevIncStatus.String != "" && prevIncStatus.String != "running" && currStatus == "running" {
			h.tg.DispatchAlert("general:"+deviceID+":cycle_start", "general", "info",
				fmt.Sprintf("Siklus inkubasi 21 hari resmi dimulai (Hari ke-1, target suhu %.1f°C). Telur akan dibalik otomatis sesuai jadwal.", p.TargetTemperature),
				1*time.Minute, notifCfg.TelegramEnabled)
		} else if currStatus == "running" && prevIncStage.String != "lockdown" && currStage == "lockdown" {
			h.tg.DispatchAlert("general:"+deviceID+":cycle_lockdown", "general", "info",
				"Siklus memasuki Fase Lockdown (Hari 19–21). Pembalikan telur otomatis dihentikan, bukaan oksigen ventilasi dinaikkan ke 40°, dan jaga kelembapan di rentang 60%–78%.",
				1*time.Minute, notifCfg.TelegramEnabled)
		} else if prevIncStatus.String == "running" && currStatus == "finished" {
			h.tg.DispatchAlert("general:"+deviceID+":cycle_finished", "general", "info",
				"Siklus inkubasi 21 hari telah selesai! Telur memasuki masa menetas. Pantau kondisi penetasan sebelum memindahkan anakan.",
				1*time.Minute, notifCfg.TelegramEnabled)
		}
	}

	// Fetch oldest pending dispatched command (created within last 60s)
	_, _ = h.db.Exec(
		`UPDATE commands SET status = 'failed', updated_at = UTC_TIMESTAMP()
		 WHERE device_id = ? AND status = 'dispatched' AND created_at < DATE_SUB(UTC_TIMESTAMP(), INTERVAL 60 SECOND)`,
		deviceID,
	)

	var pendingCmd *models.DeviceCommandEnvelope
	var cmdID int64
	var payloadJSON string
	if err := h.db.QueryRow(
		`SELECT id, payload_json FROM commands
		 WHERE device_id = ? AND status = 'dispatched'
		 ORDER BY id ASC LIMIT 1`,
		deviceID,
	).Scan(&cmdID, &payloadJSON); err == nil {
		var env models.DeviceCommandEnvelope
		if json.Unmarshal([]byte(payloadJSON), &env) == nil {
			env.ID = cmdID
			pendingCmd = &env
		}
	}

	// Fetch active device settings to sync to ESP32
	settingsSync := h.loadSettingsSync(deviceID)

	return c.JSON(fiber.Map{
		"status":          "ok",
		"pending_command": pendingCmd,
		"settings":        settingsSync,
	})
}

const (
	tempHighAlertC     = 38.5
	tempHighClearC     = 38.2
	tempLowAlertC      = 36.5
	tempLowClearC      = 36.8
	humSetterMinAlert  = 45.0
	humSetterMaxAlert  = 70.0
	humLockdownMin     = 60.0
	humLockdownMax     = 78.0
	humClearHystPct    = 2.0
	alertTgCooldown    = 5 * time.Minute
	recoveryTgCooldown = 3 * time.Minute
)

func humidityBoundsForPayload(p models.DeviceTelemetryPayload) (minAlert, maxAlert, minClear, maxClear float64, stageLabel string) {
	if p.Incubation.Stage != nil && strings.TrimSpace(*p.Incubation.Stage) == "lockdown" {
		return humLockdownMin, humLockdownMax, humLockdownMin + humClearHystPct, humLockdownMax - humClearHystPct, "Fase Lockdown"
	}
	return humSetterMinAlert, humSetterMaxAlert, humSetterMinAlert + humClearHystPct, humSetterMaxAlert - humClearHystPct, "Fase Setter"
}

func offlineAlertSeconds(staleSeconds int) int {
	if staleSeconds <= 0 {
		staleSeconds = 15
	}
	if staleSeconds*3 > 45 {
		return staleSeconds * 3
	}
	return 45
}

func (h *TelemetryHandler) reconcileAlerts(deviceID string, p models.DeviceTelemetryPayload, notifCfg models.NotificationSettings) {
	// Resolve stale communication offline alert because device just checked in
	resStale, staleErr := h.db.Exec(
		`UPDATE alerts SET status = 'resolved', resolved_at = UTC_TIMESTAMP()
		 WHERE device_id = ? AND type = 'communication' AND status = 'active'
		   AND (message LIKE 'Perangkat tidak mengirim data%' OR message LIKE 'Perangkat ESP32 tidak mengirim data%')`,
		deviceID,
	)
	if staleErr == nil {
		if affected, _ := resStale.RowsAffected(); affected > 0 && notifCfg.General && h.tg != nil {
			h.tg.DispatchAlert("recovery:"+deviceID+":communication", "general", "info",
				fmt.Sprintf("Koneksi ESP32 kembali online dan mengirim data normal (Suhu %.1f°C, Kelembapan %.0f%%).", p.Temperature, p.Humidity),
				recoveryTgCooldown, notifCfg.TelegramEnabled)
		}
	}

	hasSensorErr := false
	sensorMsg := "Sensor SHT30/SHT31 gagal membaca data 3 kali berturut-turut. Periksa koneksi kabel I2C (GPIO 21/22) dan daya modul sensor."
	for _, a := range p.ActiveAlerts {
		if a.Type == "sensor_error" {
			hasSensorErr = true
			if strings.TrimSpace(a.Message) != "" {
				sensorMsg = a.Message
			}
		} else if a.Type == "communication" {
			h.ensureActiveAlert(deviceID, "communication", defaultIfEmpty(a.Severity, "medium"), a.Message, notifCfg)
		}
	}

	if hasSensorErr {
		h.ensureActiveAlert(deviceID, "sensor_error", "medium", sensorMsg, notifCfg)
	} else {
		h.resolveAlertTypeAndNotify(deviceID, "sensor_error",
			"Pembacaan sensor suhu & kelembapan SHT30/SHT31 kembali normal.", notifCfg)
	}

	// Evaluate temperature thresholds with hysteresis (36.5 - 38.5 °C)
	if p.Temperature > tempHighAlertC {
		h.ensureActiveAlert(deviceID, "over_temperature", "high",
			fmt.Sprintf("Suhu terpantau %.1f°C (melewati batas aman atas 38.5°C). Periksa dimmer lampu pemanas dan pastikan ventilasi 90° terbuka lancar.", p.Temperature), notifCfg)
	} else if p.Temperature <= tempHighClearC {
		h.resolveAlertTypeAndNotify(deviceID, "over_temperature",
			fmt.Sprintf("Suhu kembali turun ke rentang aman (%.1f°C).", p.Temperature), notifCfg)
	}

	if p.Temperature < tempLowAlertC {
		h.ensureActiveAlert(deviceID, "low_temperature", "low",
			fmt.Sprintf("Suhu terpantau %.1f°C (di bawah batas aman bawah 36.5°C). Periksa lampu bohlam 60W, dimmer AC, dan pastikan boks tertutup rapat.", p.Temperature), notifCfg)
	} else if p.Temperature >= tempLowClearC {
		h.resolveAlertTypeAndNotify(deviceID, "low_temperature",
			fmt.Sprintf("Suhu kembali naik ke rentang aman (%.1f°C).", p.Temperature), notifCfg)
	}

	// Evaluate stage-aware humidity bounds with hysteresis (Setter 45-70%, Lockdown 60-78%)
	humMinAlert, humMaxAlert, humMinClear, humMaxClear, humStage := humidityBoundsForPayload(p)
	if !hasSensorErr && p.Humidity < humMinAlert {
		advice := "Tambahkan air pada wadah penguapan di dalam boks."
		if humStage == "Fase Lockdown" {
			advice = "Tambahkan air hangat atau perbesar luas wadah air dan jaga tutup boks tetap rapat."
		}
		h.ensureActiveAlert(deviceID, "humidity_warning", "medium",
			fmt.Sprintf("Kelembapan terpantau %.0f%% (di bawah batas minimal %s %.0f%%). %s", p.Humidity, humStage, humMinAlert, advice), notifCfg)
	} else if !hasSensorErr && p.Humidity > humMaxAlert {
		h.ensureActiveAlert(deviceID, "humidity_warning", "medium",
			fmt.Sprintf("Kelembapan terpantau %.0f%% (melewati batas maksimal %s %.0f%%). Kurangi luas air atau biarkan ventilasi mengalirkan uap berlebih.", p.Humidity, humStage, humMaxAlert), notifCfg)
	} else if !hasSensorErr && p.Humidity >= humMinClear && p.Humidity <= humMaxClear {
		h.resolveAlertTypeAndNotify(deviceID, "humidity_warning",
			fmt.Sprintf("Kelembapan kembali masuk ke rentang aman %s (%.0f%%).", humStage, p.Humidity), notifCfg)
	}

	if !p.Motor.EncoderOK || p.Turning.Status == "fault" {
		h.ensureActiveAlert(deviceID, "motor_fault", "high",
			"Motor pembalik telur menyala tetapi sensor posisi tidak mendeteksi gerakan.", notifCfg)
	} else {
		h.resolveAlertTypeAndNotify(deviceID, "motor_fault", "Sistem pembalik telur kembali berjalan normal.", notifCfg)
	}
}

func (h *TelemetryHandler) ensureActiveAlert(deviceID, alertType, severity, message string, notifCfg models.NotificationSettings) {
	var existingID int64
	err := h.db.QueryRow(
		"SELECT id FROM alerts WHERE device_id = ? AND type = ? AND status = 'active' LIMIT 1",
		deviceID, alertType,
	).Scan(&existingID)
	if err == nil {
		_, _ = h.db.Exec("UPDATE alerts SET message = ? WHERE id = ?", message, existingID)
		return
	}

	_, err = h.db.Exec(
		`INSERT INTO alerts (device_id, type, severity, message, status, created_at)
		 VALUES (?, ?, ?, ?, 'active', UTC_TIMESTAMP())`,
		deviceID, alertType, severity, message,
	)
	if err == nil && notifCfg.IsTypeEnabled(alertType) && h.tg != nil {
		h.tg.DispatchAlert("alert:"+deviceID+":"+alertType, alertType, severity, message, alertTgCooldown, notifCfg.TelegramEnabled)
	}
}

func (h *TelemetryHandler) resolveAlertTypeAndNotify(deviceID, alertType, recoveryMsg string, notifCfg models.NotificationSettings) {
	res, err := h.db.Exec(
		`UPDATE alerts SET status = 'resolved', resolved_at = UTC_TIMESTAMP()
		 WHERE device_id = ? AND type = ? AND status = 'active'`,
		deviceID, alertType,
	)
	if err == nil && recoveryMsg != "" && notifCfg.General && h.tg != nil {
		if affected, _ := res.RowsAffected(); affected > 0 {
			h.tg.DispatchAlert("recovery:"+deviceID+":"+alertType, "general", "info", recoveryMsg, recoveryTgCooldown, notifCfg.TelegramEnabled)
		}
	}
}

func (h *TelemetryHandler) loadSettingsSync(deviceID string) *models.DeviceSettingsSync {
	var sync models.DeviceSettingsSync
	var wifiSSID string
	var wifiEnc, wifiNonce sql.NullString
	var syncPending bool

	err := h.db.QueryRow(
		`SELECT target_temperature, motor_interval_minutes, motor_duration_seconds, motor_speed_percent,
		        wifi_ssid, wifi_password_enc, wifi_password_nonce, wifi_sync_pending
		 FROM device_settings WHERE device_id = ? LIMIT 1`,
		deviceID,
	).Scan(
		&sync.TargetTemperature,
		&sync.Motor.IntervalMinutes,
		&sync.Motor.DurationSeconds,
		&sync.Motor.SpeedPercent,
		&wifiSSID, &wifiEnc, &wifiNonce, &syncPending,
	)
	if err != nil {
		return nil
	}

	if syncPending && wifiEnc.Valid && wifiNonce.Valid && wifiEnc.String != "" {
		plainPass, err := crypto.DecryptAESGCM(h.cfg.EncryptionKey, wifiEnc.String, wifiNonce.String)
		if err == nil {
			sync.Wifi = &struct {
				SSID     string `json:"ssid"`
				Password string `json:"password"`
			}{
				SSID:     wifiSSID,
				Password: plainPass,
			}
			// Mark dispatched so ESP32 does not loop trying the same Wi-Fi on every 5s poll
			_, _ = h.db.Exec("UPDATE device_settings SET wifi_sync_pending = 0 WHERE device_id = ?", deviceID)
		}
	}

	return &sync
}

// GetDeviceSnapshot reads the current device_state + latest command status and evaluates staleness.
func GetDeviceSnapshot(db *sql.DB, cfg *config.Config) (models.TelemetryResponse, bool, error) {
	var resp models.TelemetryResponse
	var lastTelemetry, incStarted, incFinish, lastTurned, nextTurning sql.NullTime
	var incDay, incTotal, wifiRSSI sql.NullInt64
	var incStage sql.NullString

	err := db.QueryRow(
		`SELECT device_id, last_telemetry_at,
		        temperature, target_temperature, humidity,
		        incubation_status, incubation_current_day, incubation_total_days,
		        incubation_started_at, incubation_finish_at, incubation_stage,
		        system_health, uptime_seconds, summary,
		        lamp_mode, lamp_intensity, lamp_fuzzy_active,
		        vent_mode, vent_angle, vent_fuzzy_active,
		        motor_running, motor_encoder_ok, turning_status,
		        last_turned_at, next_turning_at,
		        wifi_connected, wifi_ssid, wifi_rssi
		 FROM device_state WHERE device_id = ? LIMIT 1`,
		cfg.DeviceID,
	).Scan(
		&resp.DeviceID, &lastTelemetry,
		&resp.Temperature, &resp.TargetTemperature, &resp.Humidity,
		&resp.Incubation.Status, &incDay, &incTotal,
		&incStarted, &incFinish, &incStage,
		&resp.DeviceCondition.SystemHealth, &resp.DeviceCondition.UptimeSeconds, &resp.DeviceCondition.Summary,
		&resp.Lamp.Mode, &resp.Lamp.Intensity, &resp.Lamp.FuzzyActive,
		&resp.Ventilation.Mode, &resp.Ventilation.Angle, &resp.Ventilation.FuzzyActive,
		&resp.Motor.Running, &resp.Motor.EncoderOK, &resp.Turning.Status,
		&lastTurned, &nextTurning,
		&resp.Wifi.Connected, &resp.Wifi.SSID, &wifiRSSI,
	)
	if err != nil {
		return resp, true, err
	}

	now := time.Now().UTC()
	resp.ServerTime = now.Format(time.RFC3339)

	isStale := true
	if lastTelemetry.Valid {
		resp.UpdatedAt = lastTelemetry.Time.UTC().Format(time.RFC3339)
		if now.Sub(lastTelemetry.Time.UTC()) <= time.Duration(cfg.StaleSeconds)*time.Second {
			isStale = false
		}
	} else {
		resp.UpdatedAt = now.Add(-1 * time.Hour).Format(time.RFC3339)
	}

	if isStale {
		resp.Connection = "offline"
	} else {
		resp.Connection = "online"
	}

	if resp.Incubation.Status != "idle" {
		if incDay.Valid {
			v := int(incDay.Int64)
			resp.Incubation.CurrentDay = &v
		}
		if incTotal.Valid {
			v := int(incTotal.Int64)
			resp.Incubation.TotalDays = &v
		}
		if incStarted.Valid {
			s := incStarted.Time.UTC().Format(time.RFC3339)
			resp.Incubation.StartedAt = &s
		}
		if incFinish.Valid {
			s := incFinish.Time.UTC().Format(time.RFC3339)
			resp.Incubation.FinishAt = &s
		}
		if incStage.Valid && incStage.String != "" {
			s := incStage.String
			resp.Incubation.Stage = &s
		}
	}

	if lastTurned.Valid {
		s := lastTurned.Time.UTC().Format(time.RFC3339)
		resp.Turning.LastTurnedAt = &s
	}
	if nextTurning.Valid && resp.Incubation.Status == "running" &&
		!isIncubationLockdown(resp.Incubation, resp.Turning) {
		s := nextTurning.Time.UTC().Format(time.RFC3339)
		resp.Turning.NextTurningAt = &s
	}

	if wifiRSSI.Valid {
		v := int(wifiRSSI.Int64)
		resp.Wifi.RSSI = &v
	}

	// Read latest command from commands table so UI immediately sees 'dispatched' -> 'executed'/'failed'
	var cmd models.LastCommandInfo
	var cmdUpdated time.Time
	if err := db.QueryRow(
		`SELECT id, target, action, status, updated_at
		 FROM commands WHERE device_id = ?
		 ORDER BY id DESC LIMIT 1`,
		cfg.DeviceID,
	).Scan(&cmd.ID, &cmd.Target, &cmd.Action, &cmd.Status, &cmdUpdated); err == nil {
		cmd.UpdatedAt = cmdUpdated.UTC().Format(time.RFC3339)
		resp.LastCommand = &cmd
	}

	return resp, isStale, nil
}

// CheckStaleDevice inspects device_state periodically and opens a communication alert after sustained offline duration.
func CheckStaleDevice(db *sql.DB, cfg *config.Config, tg *services.TelegramNotifier) {
	var lastTelemetry sql.NullTime
	err := db.QueryRow("SELECT last_telemetry_at FROM device_state WHERE device_id = ? LIMIT 1", cfg.DeviceID).Scan(&lastTelemetry)
	if err != nil || !lastTelemetry.Valid {
		return
	}

	alertSec := offlineAlertSeconds(cfg.StaleSeconds)
	elapsed := time.Since(lastTelemetry.Time.UTC())
	if elapsed > time.Duration(alertSec)*time.Second {
		var existing int64
		err := db.QueryRow(
			`SELECT id FROM alerts
			 WHERE device_id = ? AND type = 'communication' AND status = 'active'
			   AND (message LIKE 'Perangkat tidak mengirim data%' OR message LIKE 'Perangkat ESP32 tidak mengirim data%')
			 LIMIT 1`,
			cfg.DeviceID,
		).Scan(&existing)
		if err != nil {
			msg := fmt.Sprintf("Perangkat ESP32 tidak mengirim data ke server selama lebih dari %d detik. Periksa catu daya inkubator dan koneksi Wi-Fi.", alertSec)
			_, insErr := db.Exec(
				`INSERT INTO alerts (device_id, type, severity, message, status, created_at)
				 VALUES (?, 'communication', 'high', ?, 'active', UTC_TIMESTAMP())`,
				cfg.DeviceID, msg,
			)
			if insErr == nil && tg != nil {
				notifCfg := loadNotificationSettings(db, cfg, tg)
				if notifCfg.Communication {
					tg.DispatchAlert("alert:"+cfg.DeviceID+":communication", "communication", "high", msg, alertTgCooldown, notifCfg.TelegramEnabled)
				}
			}
		}
	}
}

func parseNullableTime(iso *string) sql.NullTime {
	if iso == nil || strings.TrimSpace(*iso) == "" {
		return sql.NullTime{}
	}
	t, err := parseFlexibleISO(*iso)
	if err != nil {
		return sql.NullTime{}
	}
	return sql.NullTime{Time: t.UTC(), Valid: true}
}

func toNullString(s *string) sql.NullString {
	if s == nil || strings.TrimSpace(*s) == "" {
		return sql.NullString{}
	}
	return sql.NullString{String: *s, Valid: true}
}

func defaultIfEmpty(s, fallback string) string {
	if strings.TrimSpace(s) == "" {
		return fallback
	}
	return s
}

func normalizeIncubationStage(p *models.DeviceTelemetryPayload) {
	status := defaultIfEmpty(p.Incubation.Status, "idle")
	p.Incubation.Status = status

	if status == "idle" {
		p.Incubation.CurrentDay = nil
		p.Incubation.TotalDays = nil
		p.Incubation.StartedAt = nil
		p.Incubation.FinishAt = nil
		p.Incubation.Stage = nil
		p.Turning.NextTurningAt = nil
		if p.Turning.Status == "lockdown" {
			p.Turning.Status = "idle"
		}
		return
	}

	isLockdown := status == "finished" ||
		(p.Incubation.CurrentDay != nil && *p.Incubation.CurrentDay >= 19) ||
		(p.Incubation.Stage != nil && *p.Incubation.Stage == "lockdown")

	if isLockdown {
		stage := "lockdown"
		p.Incubation.Stage = &stage
		p.Turning.NextTurningAt = nil
		if p.Turning.Status != "fault" {
			p.Turning.Status = "lockdown"
		}
		return
	}

	if p.Incubation.Stage == nil || strings.TrimSpace(*p.Incubation.Stage) == "" {
		stage := "turning"
		p.Incubation.Stage = &stage
	}
}
