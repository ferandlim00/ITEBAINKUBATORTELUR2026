package services

import (
	"strings"
	"testing"
	"time"
)

func TestTelegramNotifierConfigured(t *testing.T) {
	if NewTelegramNotifier("", "123").IsConfigured() {
		t.Fatal("expected unconfigured when botToken is empty")
	}
	if NewTelegramNotifier("abc", " ").IsConfigured() {
		t.Fatal("expected unconfigured when chatID is whitespace")
	}
	if !NewTelegramNotifier("abc", "123").IsConfigured() {
		t.Fatal("expected configured when both token and chatID are provided")
	}
}

func TestFormatAlertHeaderAndLabel(t *testing.T) {
	h1, l1 := formatAlertHeaderAndLabel("sensor_error", "medium", "Pembacaan gagal")
	if !strings.Contains(h1, "SENSOR") || !strings.Contains(l1, "SHT30/SHT31") {
		t.Fatalf("unexpected header/label for sensor_error: %q / %q", h1, l1)
	}

	h2, l2 := formatAlertHeaderAndLabel("general", "info", "Siklus inkubasi dimulai")
	if !strings.Contains(h2, "INFO INKUBATOR") || !strings.Contains(l2, "Informasi Umum") {
		t.Fatalf("unexpected header/label for general: %q / %q", h2, l2)
	}

	hRec, lRec := formatAlertHeaderAndLabel("general", "info", "Suhu kembali aman di rentang target.")
	if !strings.Contains(hRec, "PEMULIHAN") || !strings.Contains(lRec, "Stabil") {
		t.Fatalf("unexpected header/label for recovery: %q / %q", hRec, lRec)
	}

	h3, _ := formatAlertHeaderAndLabel("low_temperature", "low", "Suhu 36.1C")
	if !strings.Contains(h3, "SUHU RENDAH") {
		t.Fatalf("unexpected header for low_temperature: %q", h3)
	}

	rendered := formatTelegramHTMLMessage("over_temperature", "high", "Suhu 38.9°C terlalu panas.", time.Date(2026, 11, 26, 10, 0, 0, 0, time.UTC))
	if strings.Contains(rendered, "<code>") {
		t.Fatalf("expected human-readable message without internal <code> tags, got: %s", rendered)
	}
}

func TestTelegramCooldownAntiSpam(t *testing.T) {
	tg := NewTelegramNotifier("tok", "chat")
	fakeNow := time.Date(2026, 11, 26, 10, 0, 0, 0, time.UTC)
	tg.nowFn = func() time.Time { return fakeNow }

	if !tg.allowSend("alert:inkubator-01:over_temperature", 5*time.Minute) {
		t.Fatal("first send must be allowed")
	}
	if tg.allowSend("alert:inkubator-01:over_temperature", 5*time.Minute) {
		t.Fatal("immediate repeat for same key must be throttled")
	}
	if !tg.allowSend("alert:inkubator-01:low_temperature", 5*time.Minute) {
		t.Fatal("different alert key must not be blocked")
	}

	fakeNow = fakeNow.Add(4 * time.Minute)
	if tg.allowSend("alert:inkubator-01:over_temperature", 5*time.Minute) {
		t.Fatal("send before 5m cooldown expires must still be throttled")
	}

	fakeNow = fakeNow.Add(1 * time.Minute)
	if !tg.allowSend("alert:inkubator-01:over_temperature", 5*time.Minute) {
		t.Fatal("send after cooldown elapsed must be allowed")
	}
}
