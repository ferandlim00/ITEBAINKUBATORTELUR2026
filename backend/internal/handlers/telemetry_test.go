package handlers

import (
	"testing"

	"backend/internal/models"
)

func TestHumidityBoundsForPayload(t *testing.T) {
	setterStage := "turning"
	setterPayload := models.DeviceTelemetryPayload{
		Incubation: models.IncubationInfo{Status: "running", Stage: &setterStage},
	}
	minA, maxA, minC, maxC, label := humidityBoundsForPayload(setterPayload)
	if minA != 45 || maxA != 70 || minC != 47 || maxC != 68 || label != "Fase Setter" {
		t.Fatalf("unexpected setter bounds: %v %v %v %v %q", minA, maxA, minC, maxC, label)
	}

	lockdownStage := "lockdown"
	lockdownPayload := models.DeviceTelemetryPayload{
		Incubation: models.IncubationInfo{Status: "running", Stage: &lockdownStage},
	}
	minA, maxA, minC, maxC, label = humidityBoundsForPayload(lockdownPayload)
	if minA != 60 || maxA != 78 || minC != 62 || maxC != 76 || label != "Fase Lockdown" {
		t.Fatalf("unexpected lockdown bounds: %v %v %v %v %q", minA, maxA, minC, maxC, label)
	}
}

func TestOfflineAlertSeconds(t *testing.T) {
	if got := offlineAlertSeconds(15); got != 45 {
		t.Fatalf("offlineAlertSeconds(15) = %d, want 45", got)
	}
	if got := offlineAlertSeconds(20); got != 60 {
		t.Fatalf("offlineAlertSeconds(20) = %d, want 60", got)
	}
	if got := offlineAlertSeconds(0); got != 45 {
		t.Fatalf("offlineAlertSeconds(0) = %d, want 45", got)
	}
}
