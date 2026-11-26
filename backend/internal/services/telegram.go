package services

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"html"
	"io"
	"log"
	"net/http"
	"strings"
	"sync"
	"time"
)

var wibZone = time.FixedZone("WIB", 7*3600)

// TelegramNotifier dispatches configured incubator notifications to a Telegram chat
// with per-key cooldown throttling to prevent boundary-flap spam.
type TelegramNotifier struct {
	botToken string
	chatID   string
	fcm      *FCMNotifier
	client   *http.Client
	mu       sync.Mutex
	lastSent map[string]time.Time
	nowFn    func() time.Time
}

// NewTelegramNotifier creates a notifier that is active when both token and chat ID exist.
func NewTelegramNotifier(botToken, chatID string) *TelegramNotifier {
	return &TelegramNotifier{
		botToken: strings.TrimSpace(botToken),
		chatID:   strings.TrimSpace(chatID),
		client:   &http.Client{Timeout: 5 * time.Second},
		lastSent: make(map[string]time.Time),
		nowFn:    time.Now,
	}
}

// SetFCM attaches an optional Firebase Cloud Messaging notifier so alerts also push to Android apps.
func (t *TelegramNotifier) SetFCM(fcm *FCMNotifier) {
	if t != nil {
		t.fcm = fcm
	}
}

// HasFCM reports whether Firebase Cloud Messaging is active on this notifier.
func (t *TelegramNotifier) HasFCM() bool {
	return t != nil && t.fcm != nil && t.fcm.IsConfigured()
}

// SendTestFCM sends a synchronous test push notification via Firebase Cloud Messaging.
func (t *TelegramNotifier) SendTestFCM(username string) error {
	if !t.HasFCM() {
		return errors.New("Firebase Cloud Messaging belum aktif di server.")
	}
	return t.fcm.SendTest(username)
}

// IsConfigured reports whether TELEGRAM_BOT_TOKEN and TELEGRAM_CHAT_ID are both set.
func (t *TelegramNotifier) IsConfigured() bool {
	return t != nil && t.botToken != "" && t.chatID != ""
}

// allowSend checks and records per-key cooldown windows to suppress repeated Telegram spam.
func (t *TelegramNotifier) allowSend(cooldownKey string, cooldown time.Duration) bool {
	if cooldown <= 0 || strings.TrimSpace(cooldownKey) == "" {
		return true
	}
	t.mu.Lock()
	defer t.mu.Unlock()

	now := t.nowFn()
	if prev, ok := t.lastSent[cooldownKey]; ok && now.Sub(prev) < cooldown {
		return false
	}
	t.lastSent[cooldownKey] = now
	return true
}

// SendAlert asynchronously sends a formatted notification to Telegram using alertType as cooldown key.
func (t *TelegramNotifier) SendAlert(alertType, severity, message string) {
	t.SendWithCooldown("alert:"+alertType, alertType, severity, message, 5*time.Minute)
}

// SendWithCooldown asynchronously sends a notification unless the same cooldownKey fired too recently.
func (t *TelegramNotifier) SendWithCooldown(cooldownKey, alertType, severity, message string, cooldown time.Duration) {
	t.DispatchAlert(cooldownKey, alertType, severity, message, cooldown, true)
}

// DispatchAlert sends to FCM (always when enabled for the alert type) and to Telegram (when telegramEnabled is true).
func (t *TelegramNotifier) DispatchAlert(cooldownKey, alertType, severity, message string, cooldown time.Duration, telegramEnabled bool) {
	if t == nil {
		return
	}
	sendTG := telegramEnabled && t.IsConfigured()
	sendFCM := t.HasFCM()
	if !sendTG && !sendFCM {
		return
	}
	if !t.allowSend(cooldownKey, cooldown) {
		return
	}
	if sendTG {
		go func() {
			if err := t.sendFormattedHTML(alertType, severity, message); err != nil {
				log.Printf("[TELEGRAM] Gagal kirim notifikasi (%s): %v", alertType, err)
			}
		}()
	}
	if sendFCM {
		go func() {
			if err := t.fcm.SendTopicAlert(alertType, severity, message); err != nil {
				log.Printf("[FCM] Gagal kirim push notification (%s): %v", alertType, err)
			}
		}()
	}
}

// SendHighAlert preserves backwards compatibility for high-severity callers.
func (t *TelegramNotifier) SendHighAlert(alertType, message string) {
	t.SendAlert(alertType, "high", message)
}

// SendTest synchronously sends a test notification so operator test requests bypass cooldowns.
func (t *TelegramNotifier) SendTest(username string) error {
	if !t.IsConfigured() {
		return errors.New("TELEGRAM_BOT_TOKEN dan TELEGRAM_CHAT_ID belum diisi pada konfigurasi server.")
	}
	who := strings.TrimSpace(username)
	if who == "" {
		who = "operator"
	}
	msg := fmt.Sprintf("Uji koneksi Bot Telegram Smart Hatchery berhasil dijalankan oleh %s. Notifikasi siap digunakan.", who)
	return t.sendFormattedHTML("general", "info", msg)
}

func (t *TelegramNotifier) sendFormattedHTML(alertType, severity, message string) error {
	text := formatTelegramHTMLMessage(alertType, severity, message, t.nowFn())

	body, err := json.Marshal(map[string]any{
		"chat_id":    t.chatID,
		"text":       text,
		"parse_mode": "HTML",
	})
	if err != nil {
		return err
	}

	url := fmt.Sprintf("https://api.telegram.org/bot%s/sendMessage", t.botToken)
	resp, err := t.client.Post(url, "application/json", bytes.NewReader(body))
	if err != nil {
		return fmt.Errorf("koneksi ke Telegram API gagal: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		raw, _ := io.ReadAll(io.LimitReader(resp.Body, 512))
		return fmt.Errorf("Telegram API menolak permintaan (HTTP %d): %s", resp.StatusCode, strings.TrimSpace(string(raw)))
	}
	return nil
}

func formatTelegramHTMLMessage(alertType, severity, message string, at time.Time) string {
	header, jenisLabel := formatAlertHeaderAndLabel(alertType, severity, message)
	waktuWIB := at.In(wibZone).Format("02/01/2006 15:04:05 WIB")

	return fmt.Sprintf("%s\n\n<b>Status:</b> %s\n<b>Keterangan:</b> %s\n<b>Waktu:</b> %s",
		header,
		html.EscapeString(jenisLabel),
		html.EscapeString(strings.TrimSpace(message)),
		html.EscapeString(waktuWIB),
	)
}

func formatAlertHeaderAndLabel(alertType, severity, message string) (string, string) {
	labels := map[string]string{
		"over_temperature": "Suhu Terlalu Panas (Melewati 38.5°C)",
		"low_temperature":  "Suhu Terlalu Dingin (Di Bawah 36.5°C)",
		"sensor_error":     "Sensor Suhu & Kelembapan SHT30/SHT31 Gagal Baca",
		"humidity_warning": "Kelembapan Di Luar Rentang Aman",
		"communication":    "Gangguan Komunikasi ESP32 / Wi-Fi",
		"motor_fault":      "Gangguan Sistem Pembalik Telur",
		"general":          "Informasi Umum & Siklus Inkubasi",
	}
	label := labels[alertType]
	if label == "" {
		label = "Notifikasi Sistem Inkubator"
	}

	if alertType == "general" || severity == "info" {
		lower := strings.ToLower(message)
		if strings.Contains(lower, "kembali") || strings.Contains(lower, "pulih") || strings.Contains(lower, "normal") {
			return "✅ <b>INFO PEMULIHAN INKUBATOR</b>", "Kondisi Kembali Stabil"
		}
		return "ℹ️ <b>INFO INKUBATOR SMART HATCHERY</b>", label
	}

	switch alertType {
	case "over_temperature":
		return "🚨 <b>PERINGATAN SUHU TINGGI (DARURAT)</b>", label
	case "low_temperature":
		return "❄️ <b>PERINGATAN SUHU RENDAH</b>", label
	case "humidity_warning":
		return "💧 <b>PERINGATAN KELEMBAPAN</b>", label
	case "sensor_error":
		return "🔌 <b>GANGGUAN SENSOR SHT30/SHT31</b>", label
	case "communication":
		return "📡 <b>KONEKSI PERANGKAT TERPUTUS</b>", label
	case "motor_fault":
		return "⚙️ <b>GANGGUAN PEMBALIK TELUR</b>", label
	}

	switch severity {
	case "high":
		return "🚨 <b>PERINGATAN DARURAT INKUBATOR</b>", label
	case "medium":
		return "⚠️ <b>PERINGATAN INKUBATOR</b>", label
	default:
		return "ℹ️ <b>PEMBERITAHUAN INKUBATOR</b>", label
	}
}
