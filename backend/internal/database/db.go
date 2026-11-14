package database

import (
	"database/sql"
	"fmt"
	"strings"
	"time"

	"backend/internal/config"
	"backend/internal/crypto"

	_ "github.com/go-sql-driver/mysql"
)

// Connect opens a MySQL connection pool, ensures database & tables exist, and seeds initial records.
func Connect(cfg *config.Config) (*sql.DB, error) {
	safeDBName := strings.ReplaceAll(cfg.DBName, "`", "")
	appDSN := fmt.Sprintf("%s:%s@tcp(%s:%s)/%s?parseTime=true&loc=UTC&charset=utf8mb4",
		cfg.DBUser, cfg.DBPassword, cfg.DBHost, cfg.DBPort, safeDBName)

	// 1. Try connecting directly to the target database first (works with restricted VPS/Docker DB users).
	db, err := sql.Open("mysql", appDSN)
	if err != nil {
		return nil, err
	}
	if pingErr := db.Ping(); pingErr != nil {
		_ = db.Close()

		// 2. Fallback: connect without DB name and create the database if missing (e.g. fresh local XAMPP).
		rootDSN := fmt.Sprintf("%s:%s@tcp(%s:%s)/?parseTime=true&loc=UTC&charset=utf8mb4",
			cfg.DBUser, cfg.DBPassword, cfg.DBHost, cfg.DBPort)
		rootDB, rootErr := sql.Open("mysql", rootDSN)
		if rootErr != nil {
			return nil, rootErr
		}
		if err := rootDB.Ping(); err != nil {
			_ = rootDB.Close()
			return nil, fmt.Errorf("gagal terhubung ke MySQL (%s:%s): %w", cfg.DBHost, cfg.DBPort, err)
		}
		if _, err := rootDB.Exec(fmt.Sprintf("CREATE DATABASE IF NOT EXISTS `%s` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci", safeDBName)); err != nil {
			_ = rootDB.Close()
			return nil, fmt.Errorf("gagal membuat database %s: %w", safeDBName, err)
		}
		_ = rootDB.Close()

		db, err = sql.Open("mysql", appDSN)
		if err != nil {
			return nil, err
		}
		if err := db.Ping(); err != nil {
			_ = db.Close()
			return nil, err
		}
	}

	db.SetMaxOpenConns(20)
	db.SetMaxIdleConns(5)
	db.SetConnMaxLifetime(30 * time.Minute)

	if err := runMigrations(db); err != nil {
		_ = db.Close()
		return nil, err
	}
	if err := seedDefaults(db, cfg); err != nil {
		_ = db.Close()
		return nil, err
	}

	return db, nil
}

func runMigrations(db *sql.DB) error {
	statements := []string{
		`CREATE TABLE IF NOT EXISTS users (
			id BIGINT AUTO_INCREMENT PRIMARY KEY,
			username VARCHAR(64) NOT NULL UNIQUE,
			password_hash TEXT NOT NULL,
			role VARCHAR(32) NOT NULL DEFAULT 'operator',
			created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP
		) ENGINE=InnoDB`,

		`CREATE TABLE IF NOT EXISTS sessions (
			id BIGINT AUTO_INCREMENT PRIMARY KEY,
			token_hash CHAR(64) NOT NULL UNIQUE,
			user_id BIGINT NOT NULL,
			expires_at DATETIME NOT NULL,
			created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
			INDEX idx_sessions_expires (expires_at),
			CONSTRAINT fk_sessions_user FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
		) ENGINE=InnoDB`,

		`CREATE TABLE IF NOT EXISTS device_settings (
			device_id VARCHAR(64) PRIMARY KEY,
			target_temperature DECIMAL(4,2) NOT NULL DEFAULT 37.50,
			motor_interval_minutes INT NOT NULL DEFAULT 120,
			motor_duration_seconds INT NOT NULL DEFAULT 20,
			motor_speed_percent INT NOT NULL DEFAULT 50,
			wifi_ssid VARCHAR(32) NOT NULL DEFAULT 'IncubatorNet',
			wifi_password_enc TEXT NULL,
			wifi_password_nonce VARCHAR(64) NULL,
			wifi_sync_pending TINYINT(1) NOT NULL DEFAULT 0,
			updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
		) ENGINE=InnoDB`,

		`CREATE TABLE IF NOT EXISTS device_state (
			device_id VARCHAR(64) PRIMARY KEY,
			last_telemetry_at DATETIME NULL,
			device_updated_at DATETIME NULL,
			temperature DECIMAL(5,2) NOT NULL DEFAULT 37.50,
			target_temperature DECIMAL(5,2) NOT NULL DEFAULT 37.50,
			humidity DECIMAL(5,2) NOT NULL DEFAULT 58.00,
			incubation_status VARCHAR(16) NOT NULL DEFAULT 'idle',
			incubation_current_day INT NULL,
			incubation_total_days INT NULL,
			incubation_started_at DATETIME NULL,
			incubation_finish_at DATETIME NULL,
			incubation_stage VARCHAR(16) NULL,
			system_health VARCHAR(16) NOT NULL DEFAULT 'normal',
			uptime_seconds BIGINT UNSIGNED NOT NULL DEFAULT 0,
			summary VARCHAR(255) NOT NULL DEFAULT 'Menunggu data pertama dari perangkat ESP32.',
			lamp_mode VARCHAR(16) NOT NULL DEFAULT 'auto',
			lamp_intensity INT NOT NULL DEFAULT 0,
			lamp_fuzzy_active TINYINT(1) NOT NULL DEFAULT 1,
			vent_mode VARCHAR(16) NOT NULL DEFAULT 'auto',
			vent_angle INT NOT NULL DEFAULT 0,
			vent_fuzzy_active TINYINT(1) NOT NULL DEFAULT 1,
			motor_running TINYINT(1) NOT NULL DEFAULT 0,
			motor_encoder_ok TINYINT(1) NOT NULL DEFAULT 1,
			turning_status VARCHAR(16) NOT NULL DEFAULT 'idle',
			last_turned_at DATETIME NULL,
			next_turning_at DATETIME NULL,
			wifi_connected TINYINT(1) NOT NULL DEFAULT 0,
			wifi_ssid VARCHAR(32) NOT NULL DEFAULT '',
			wifi_rssi INT NULL,
			last_command_id BIGINT NULL,
			last_command_target VARCHAR(32) NULL,
			last_command_action VARCHAR(32) NULL,
			last_command_status VARCHAR(16) NULL,
			last_command_updated_at DATETIME NULL
		) ENGINE=InnoDB`,

		`CREATE TABLE IF NOT EXISTS telemetry_history (
			id BIGINT AUTO_INCREMENT PRIMARY KEY,
			device_id VARCHAR(64) NOT NULL,
			temperature DECIMAL(5,2) NOT NULL,
			humidity DECIMAL(5,2) NOT NULL,
			lamp_intensity INT NOT NULL,
			vent_angle INT NOT NULL DEFAULT 0,
			recorded_at DATETIME NOT NULL,
			INDEX idx_history_device_time (device_id, recorded_at)
		) ENGINE=InnoDB`,

		`CREATE TABLE IF NOT EXISTS commands (
			id BIGINT AUTO_INCREMENT PRIMARY KEY,
			device_id VARCHAR(64) NOT NULL,
			user_id BIGINT NULL,
			target VARCHAR(32) NOT NULL,
			action VARCHAR(32) NOT NULL,
			payload_json TEXT NOT NULL,
			status VARCHAR(16) NOT NULL DEFAULT 'dispatched',
			created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
			updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
			INDEX idx_commands_device_status (device_id, status, id)
		) ENGINE=InnoDB`,

		`CREATE TABLE IF NOT EXISTS alerts (
			id BIGINT AUTO_INCREMENT PRIMARY KEY,
			device_id VARCHAR(64) NOT NULL,
			type VARCHAR(32) NOT NULL,
			severity VARCHAR(16) NOT NULL,
			message VARCHAR(255) NOT NULL,
			status VARCHAR(16) NOT NULL DEFAULT 'active',
			created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
			resolved_at DATETIME NULL,
			INDEX idx_alerts_status_created (device_id, status, created_at)
		) ENGINE=InnoDB`,

		`CREATE TABLE IF NOT EXISTS audit_logs (
			id BIGINT AUTO_INCREMENT PRIMARY KEY,
			username VARCHAR(64) NOT NULL,
			event VARCHAR(32) NOT NULL,
			detail VARCHAR(255) NOT NULL,
			created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
			INDEX idx_audit_created (created_at),
			INDEX idx_audit_event (event, created_at)
		) ENGINE=InnoDB`,
	}

	for _, stmt := range statements {
		if _, err := db.Exec(stmt); err != nil {
			return fmt.Errorf("migrasi gagal: %w", err)
		}
	}
	return nil
}

func seedDefaults(db *sql.DB, cfg *config.Config) error {
	var userCount int
	if err := db.QueryRow("SELECT COUNT(*) FROM users").Scan(&userCount); err != nil {
		return err
	}
	if userCount == 0 {
		hash, err := crypto.HashPassword(cfg.OperatorPassword)
		if err != nil {
			return err
		}
		if _, err := db.Exec(
			"INSERT INTO users (username, password_hash, role) VALUES (?, ?, 'operator')",
			cfg.OperatorUsername, hash,
		); err != nil {
			return err
		}
	}

	if _, err := db.Exec(
		`INSERT IGNORE INTO device_settings (device_id, target_temperature, motor_interval_minutes, motor_duration_seconds, motor_speed_percent, wifi_ssid)
		 VALUES (?, 37.50, 120, 20, 50, 'IncubatorNet')`,
		cfg.DeviceID,
	); err != nil {
		return err
	}

	if _, err := db.Exec(
		`INSERT IGNORE INTO device_state (device_id, summary) VALUES (?, 'Menunggu koneksi pertama dari perangkat ESP32.')`,
		cfg.DeviceID,
	); err != nil {
		return err
	}

	return nil
}

// WriteAuditLog records an operator or security event into audit_logs.
func WriteAuditLog(db *sql.DB, username, event, detail string) {
	if username == "" {
		username = "sistem"
	}
	_, _ = db.Exec(
		"INSERT INTO audit_logs (username, event, detail, created_at) VALUES (?, ?, ?, UTC_TIMESTAMP())",
		username, event, detail,
	)
}
