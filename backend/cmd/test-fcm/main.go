package main

import (
	"flag"
	"fmt"
	"log"
	"os"

	"backend/internal/config"
	"backend/internal/services"
)

func main() {
	credsFlag := flag.String("creds", "", "Path ke file JSON service account Firebase (opsional, default dari FIREBASE_CREDENTIALS_FILE)")
	topicFlag := flag.String("topic", "", "Topic FCM tujuan (opsional, default dari FCM_TOPIC atau incubator_alerts)")
	alertType := flag.String("type", "general", "Jenis alert: over_temperature | low_temperature | sensor_error | humidity_warning | communication | motor_fault | general")
	severity := flag.String("severity", "info", "Tingkat severity: high | medium | low | info")
	message := flag.String("message", "Uji kirim Firebase Cloud Messaging Smart Hatchery berhasil!", "Isi pesan notifikasi uji")
	flag.Parse()

	cfg, err := config.Load()
	if err != nil {
		log.Fatalf("Gagal memuat konfigurasi: %v", err)
	}

	credsPath := cfg.FirebaseCredentialsFile
	if *credsFlag != "" {
		credsPath = *credsFlag
	}
	topic := cfg.FCMTopic
	if *topicFlag != "" {
		topic = *topicFlag
	}

	fcm := services.NewFCMNotifier(credsPath, topic)
	if fcm == nil || !fcm.IsConfigured() {
		fmt.Fprintf(os.Stderr, "GAGAL: Kredensial Firebase tidak ditemukan atau tidak valid di %q\n", credsPath)
		fmt.Fprintln(os.Stderr, "Pastikan file firebase-service-account.json ada, atau set FIREBASE_CREDENTIALS_FILE / -creds <path>.")
		os.Exit(1)
	}

	fmt.Printf("Mengirim uji FCM ke topic=%q (type=%s, severity=%s)...\n", topic, *alertType, *severity)
	if err := fcm.SendTopicAlert(*alertType, *severity, *message); err != nil {
		log.Fatalf("GAGAL mengirim FCM: %v", err)
	}

	fmt.Println("BERHASIL: Notifikasi Firebase Cloud Messaging terkirim!")
}
