package services

import (
	"bytes"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"time"
)

// TelegramNotifier dispatches high-severity alerts to a Telegram chat if configured.
type TelegramNotifier struct {
	botToken string
	chatID   string
	client   *http.Client
}

// NewTelegramNotifier creates a notifier that is active only when both token and chat ID exist.
func NewTelegramNotifier(botToken, chatID string) *TelegramNotifier {
	return &TelegramNotifier{
		botToken: botToken,
		chatID:   chatID,
		client:   &http.Client{Timeout: 5 * time.Second},
	}
}

// SendHighAlert asynchronously sends a formatted alert message to Telegram.
func (t *TelegramNotifier) SendHighAlert(alertType, message string) {
	if t.botToken == "" || t.chatID == "" {
		return
	}

	go func() {
		text := fmt.Sprintf("🚨 *PERINGATAN INKUBATOR (TINGGI)*\nJenis: `%s`\nPesan: %s\nWaktu (UTC): %s",
			alertType, message, time.Now().UTC().Format(time.RFC3339))
		body, _ := json.Marshal(map[string]any{
			"chat_id":    t.chatID,
			"text":       text,
			"parse_mode": "Markdown",
		})
		url := fmt.Sprintf("https://api.telegram.org/bot%s/sendMessage", t.botToken)
		resp, err := t.client.Post(url, "application/json", bytes.NewReader(body))
		if err != nil {
			log.Printf("[TELEGRAM] Gagal kirim notifikasi: %v", err)
			return
		}
		_ = resp.Body.Close()
	}()
}
