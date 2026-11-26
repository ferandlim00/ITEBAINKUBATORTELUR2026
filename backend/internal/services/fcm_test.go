package services

import (
	"crypto/rand"
	"crypto/rsa"
	"strings"
	"testing"
	"time"
)

func TestFCMNotifierConfigured(t *testing.T) {
	var nilFCM *FCMNotifier
	if nilFCM.IsConfigured() {
		t.Fatal("nil FCMNotifier must not be configured")
	}
	if (&FCMNotifier{projectID: "demo"}).IsConfigured() {
		t.Fatal("FCMNotifier without privateKey must not be configured")
	}

	key, err := rsa.GenerateKey(rand.Reader, 1024)
	if err != nil {
		t.Fatalf("failed to generate test rsa key: %v", err)
	}
	fcm := &FCMNotifier{
		projectID:  "parkir-cerdas-realtime",
		privateKey: key,
		topic:      DefaultFCMTopic,
	}
	if !fcm.IsConfigured() {
		t.Fatal("expected FCMNotifier with projectID and privateKey to be configured")
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
}

func TestFCMCooldownAntiSpam(t *testing.T) {
	fcm := &FCMNotifier{
		cooldowns: make(map[string]time.Time),
	}
	fakeNow := time.Date(2026, 11, 26, 10, 0, 0, 0, time.UTC)
	fcm.nowFn = func() time.Time { return fakeNow }

	if !fcm.allowSend("alert:inkubator-01:over_temperature", 5*time.Minute) {
		t.Fatal("first send must be allowed")
	}
	if fcm.allowSend("alert:inkubator-01:over_temperature", 5*time.Minute) {
		t.Fatal("immediate repeat for same key must be throttled")
	}
	if !fcm.allowSend("alert:inkubator-01:low_temperature", 5*time.Minute) {
		t.Fatal("different alert key must not be blocked")
	}

	fakeNow = fakeNow.Add(4 * time.Minute)
	if fcm.allowSend("alert:inkubator-01:over_temperature", 5*time.Minute) {
		t.Fatal("send before 5m cooldown expires must still be throttled")
	}

	fakeNow = fakeNow.Add(1 * time.Minute)
	if !fcm.allowSend("alert:inkubator-01:over_temperature", 5*time.Minute) {
		t.Fatal("send after cooldown elapsed must be allowed")
	}
}
