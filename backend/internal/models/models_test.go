package models

import "testing"

func TestNotificationSettingsIsTypeEnabled(t *testing.T) {
	cfg := NotificationSettings{
		OverTemperature: true,
		LowTemperature:  false,
		SensorError:     true,
		HumidityWarning: false,
		Communication:   true,
		MotorFault:      false,
		General:         true,
	}

	cases := []struct {
		alertType string
		want      bool
	}{
		{"over_temperature", true},
		{"low_temperature", false},
		{"sensor_error", true},
		{"humidity_warning", false},
		{"communication", true},
		{"motor_fault", false},
		{"general", true},
		{"unknown_type", true},
	}

	for _, tc := range cases {
		if got := cfg.IsTypeEnabled(tc.alertType); got != tc.want {
			t.Errorf("IsTypeEnabled(%q) = %v, want %v", tc.alertType, got, tc.want)
		}
	}
}
