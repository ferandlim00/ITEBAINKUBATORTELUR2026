package models

// User represents the authenticated Operator account.
type User struct {
	ID       int64  `json:"id"`
	Username string `json:"username"`
	Role     string `json:"role"`
}

// IncubationInfo models the 21-day incubation cycle state.
type IncubationInfo struct {
	Status     string  `json:"status"`
	CurrentDay *int    `json:"current_day"`
	TotalDays  *int    `json:"total_days"`
	StartedAt  *string `json:"started_at"`
	FinishAt   *string `json:"finish_at"`
	Stage      *string `json:"stage"`
}

// DeviceCondition models system operational health summary.
type DeviceCondition struct {
	SystemHealth  string `json:"system_health"`
	UptimeSeconds uint64 `json:"uptime_seconds"`
	Summary       string `json:"summary"`
}

// LampState models heater lamp mode and intensity.
type LampState struct {
	Mode        string `json:"mode"`
	Intensity   int    `json:"intensity"`
	FuzzyActive bool   `json:"fuzzy_active"`
}

// VentilationState models servo ventilation mode and opening angle.
type VentilationState struct {
	Mode        string `json:"mode"`
	Angle       int    `json:"angle"`
	FuzzyActive bool   `json:"fuzzy_active"`
}

// MotorState models turning motor running status and encoder health.
type MotorState struct {
	Running   bool `json:"running"`
	EncoderOK bool `json:"encoder_ok"`
}

// TurningState models turning schedule timestamps and status.
type TurningState struct {
	Status        string  `json:"status"`
	LastTurnedAt  *string `json:"last_turned_at"`
	NextTurningAt *string `json:"next_turning_at"`
}

// WifiState models active Wi-Fi connection without password exposure.
type WifiState struct {
	Connected bool   `json:"connected"`
	SSID      string `json:"ssid"`
	RSSI      *int   `json:"rssi"`
}

// LastCommandInfo models the execution status of the latest command on ESP32.
type LastCommandInfo struct {
	ID        int64  `json:"id"`
	Target    string `json:"target"`
	Action    string `json:"action"`
	Status    string `json:"status"`
	UpdatedAt string `json:"updated_at"`
}

// TelemetryResponse is the contract response for GET /api/telemetry/latest.
type TelemetryResponse struct {
	DeviceID          string           `json:"device_id"`
	Connection        string           `json:"connection"`
	UpdatedAt         string           `json:"updated_at"`
	ServerTime        string           `json:"server_time"`
	Temperature       float64          `json:"temperature"`
	TargetTemperature float64          `json:"target_temperature"`
	Humidity          float64          `json:"humidity"`
	Incubation        IncubationInfo   `json:"incubation"`
	DeviceCondition   DeviceCondition  `json:"device_condition"`
	Lamp              LampState        `json:"lamp"`
	Ventilation       VentilationState `json:"ventilation"`
	Motor             MotorState       `json:"motor"`
	Turning           TurningState     `json:"turning"`
	Wifi              WifiState        `json:"wifi"`
	LastCommand       *LastCommandInfo `json:"last_command"`
}

// DeviceTelemetryPayload is sent every 5 seconds by ESP32 via POST /api/device/telemetry.
type DeviceTelemetryPayload struct {
	DeviceID          string           `json:"device_id"`
	Connection        string           `json:"connection"`
	UpdatedAt         string           `json:"updated_at"`
	Temperature       float64          `json:"temperature"`
	TargetTemperature float64          `json:"target_temperature"`
	Humidity          float64          `json:"humidity"`
	Incubation        IncubationInfo   `json:"incubation"`
	DeviceCondition   DeviceCondition  `json:"device_condition"`
	Lamp              LampState        `json:"lamp"`
	Ventilation       VentilationState `json:"ventilation"`
	Motor             MotorState       `json:"motor"`
	Turning           TurningState     `json:"turning"`
	Wifi              WifiState        `json:"wifi"`
	LastCommand       *LastCommandInfo `json:"last_command"`
	ActiveAlerts      []AlertItem      `json:"active_alerts"`
}

// HistoryPoint represents one aggregated point for GET /api/telemetry/history.
type HistoryPoint struct {
	T             string  `json:"t"`
	Temperature   float64 `json:"temperature"`
	Humidity      float64 `json:"humidity"`
	LampIntensity int     `json:"lamp_intensity"`
}

// AlertItem models an alert returned by GET /api/alerts or reported by ESP32.
type AlertItem struct {
	ID        int64  `json:"id,omitempty"`
	Type      string `json:"type"`
	Severity  string `json:"severity"`
	Message   string `json:"message"`
	Timestamp string `json:"timestamp"`
	Status    string `json:"status"`
}

// AuditLogItem models an entry returned by GET /api/audit-logs.
type AuditLogItem struct {
	ID        int64  `json:"id"`
	User      string `json:"user"`
	Event     string `json:"event"`
	Detail    string `json:"detail"`
	Timestamp string `json:"timestamp"`
}

// CommandRequest models POST /api/commands payload.
type CommandRequest struct {
	Target    string `json:"target"`
	Action    string `json:"action,omitempty"`
	Mode      string `json:"mode,omitempty"`
	Intensity *int   `json:"intensity,omitempty"`
	Angle     *int   `json:"angle,omitempty"`
}

// DeviceCommandEnvelope is returned to ESP32 inside telemetry ACK.
type DeviceCommandEnvelope struct {
	ID        int64  `json:"id"`
	Target    string `json:"target"`
	Action    string `json:"action,omitempty"`
	Mode      string `json:"mode,omitempty"`
	Intensity *int   `json:"intensity,omitempty"`
	Angle     *int   `json:"angle,omitempty"`
}

// MotorSettings models motor parameters in /api/settings/device and PUT /api/settings/motor.
type MotorSettings struct {
	IntervalMinutes int `json:"interval_minutes"`
	DurationSeconds int `json:"duration_seconds"`
	SpeedPercent    int `json:"speed_percent"`
}

// NotificationSettings models per-type notification switches for Firebase Cloud Messaging and browser alerts.
type NotificationSettings struct {
	FCMEnabled      bool `json:"fcm_enabled"`
	FCMConfigured   bool `json:"fcm_configured"`
	OverTemperature bool `json:"over_temperature"`
	LowTemperature  bool `json:"low_temperature"`
	SensorError     bool `json:"sensor_error"`
	HumidityWarning bool `json:"humidity_warning"`
	Communication   bool `json:"communication"`
	MotorFault      bool `json:"motor_fault"`
	General         bool `json:"general"`
}

// IsTypeEnabled reports whether notifications are enabled for the given alert type.
func (n NotificationSettings) IsTypeEnabled(alertType string) bool {
	switch alertType {
	case "over_temperature":
		return n.OverTemperature
	case "low_temperature":
		return n.LowTemperature
	case "sensor_error":
		return n.SensorError
	case "humidity_warning":
		return n.HumidityWarning
	case "communication":
		return n.Communication
	case "motor_fault":
		return n.MotorFault
	case "general":
		return n.General
	default:
		return n.General
	}
}

// DeviceSettingsResponse models GET /api/settings/device (never exposes Wi-Fi password).
type DeviceSettingsResponse struct {
	TargetTemperature float64       `json:"target_temperature"`
	Motor             MotorSettings `json:"motor"`
	Wifi              struct {
		SSID string `json:"ssid"`
	} `json:"wifi"`
}

// DeviceSettingsSync is sent to ESP32 inside telemetry ACK when settings exist/update.
type DeviceSettingsSync struct {
	TargetTemperature float64       `json:"target_temperature"`
	Motor             MotorSettings `json:"motor"`
	Wifi              *struct {
		SSID     string `json:"ssid"`
		Password string `json:"password"`
	} `json:"wifi,omitempty"`
}
