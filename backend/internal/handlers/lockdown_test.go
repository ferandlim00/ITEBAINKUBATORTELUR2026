package handlers

import (
	"testing"

	"backend/internal/models"

	"github.com/gofiber/fiber/v3"
)

func intPtr(v int) *int       { return &v }
func strPtr(v string) *string { return &v }

func TestValidateCommandTurnNowLockdownRules(t *testing.T) {
	turnReq := models.CommandRequest{Target: "turning", Action: "turn_now"}

	tests := []struct {
		name       string
		snap       models.TelemetryResponse
		wantStatus int
		wantAction string
	}{
		{
			name: "allowed on day 18 turning",
			snap: models.TelemetryResponse{
				Incubation: models.IncubationInfo{Status: "running", CurrentDay: intPtr(18), Stage: strPtr("turning")},
				Motor:      models.MotorState{Running: false, EncoderOK: true},
				Turning:    models.TurningState{Status: "idle"},
			},
			wantStatus: 0,
			wantAction: "turn_now",
		},
		{
			name: "rejected when stage is lockdown",
			snap: models.TelemetryResponse{
				Incubation: models.IncubationInfo{Status: "running", CurrentDay: intPtr(19), Stage: strPtr("lockdown")},
				Motor:      models.MotorState{Running: false, EncoderOK: true},
				Turning:    models.TurningState{Status: "lockdown"},
			},
			wantStatus: fiber.StatusConflict,
		},
		{
			name: "rejected when turning status is lockdown even if stage nil",
			snap: models.TelemetryResponse{
				Incubation: models.IncubationInfo{Status: "running", CurrentDay: intPtr(19), Stage: nil},
				Motor:      models.MotorState{Running: false, EncoderOK: true},
				Turning:    models.TurningState{Status: "lockdown"},
			},
			wantStatus: fiber.StatusConflict,
		},
		{
			name: "rejected when current_day >= 19 even if stage says turning",
			snap: models.TelemetryResponse{
				Incubation: models.IncubationInfo{Status: "running", CurrentDay: intPtr(19), Stage: strPtr("turning")},
				Motor:      models.MotorState{Running: false, EncoderOK: true},
				Turning:    models.TurningState{Status: "idle"},
			},
			wantStatus: fiber.StatusConflict,
		},
		{
			name: "rejected when finished",
			snap: models.TelemetryResponse{
				Incubation: models.IncubationInfo{Status: "finished", CurrentDay: intPtr(21), Stage: strPtr("lockdown")},
				Motor:      models.MotorState{Running: false, EncoderOK: true},
				Turning:    models.TurningState{Status: "lockdown"},
			},
			wantStatus: fiber.StatusConflict,
		},
		{
			name: "rejected when idle",
			snap: models.TelemetryResponse{
				Incubation: models.IncubationInfo{Status: "idle"},
				Motor:      models.MotorState{Running: false, EncoderOK: true},
				Turning:    models.TurningState{Status: "idle"},
			},
			wantStatus: fiber.StatusConflict,
		},
		{
			name: "rejected when motor fault",
			snap: models.TelemetryResponse{
				Incubation: models.IncubationInfo{Status: "running", CurrentDay: intPtr(10), Stage: strPtr("turning")},
				Motor:      models.MotorState{Running: false, EncoderOK: false},
				Turning:    models.TurningState{Status: "fault"},
			},
			wantStatus: fiber.StatusConflict,
		},
		{
			name: "rejected when motor already running",
			snap: models.TelemetryResponse{
				Incubation: models.IncubationInfo{Status: "running", CurrentDay: intPtr(10), Stage: strPtr("turning")},
				Motor:      models.MotorState{Running: true, EncoderOK: true},
				Turning:    models.TurningState{Status: "turning"},
			},
			wantStatus: fiber.StatusConflict,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			status, action, _, errMsg := validateCommand(tc.snap, turnReq)
			if status != tc.wantStatus {
				t.Fatalf("expected status %d, got %d (errMsg=%q)", tc.wantStatus, status, errMsg)
			}
			if tc.wantStatus == 0 && (action != tc.wantAction || errMsg != "") {
				t.Fatalf("expected action %q without error, got action=%q errMsg=%q", tc.wantAction, action, errMsg)
			}
			if tc.wantStatus != 0 && errMsg == "" {
				t.Fatalf("expected Indonesian error message for status %d", tc.wantStatus)
			}
		})
	}
}

func TestNormalizeIncubationStage(t *testing.T) {
	t.Run("day 19 enforces lockdown and clears next_turning_at", func(t *testing.T) {
		p := models.DeviceTelemetryPayload{
			Incubation: models.IncubationInfo{
				Status:     "running",
				CurrentDay: intPtr(19),
				TotalDays:  intPtr(21),
				Stage:      strPtr("turning"),
			},
			Turning: models.TurningState{
				Status:        "idle",
				NextTurningAt: strPtr("2026-11-20T12:00:00Z"),
			},
		}
		normalizeIncubationStage(&p)

		if p.Incubation.Stage == nil || *p.Incubation.Stage != "lockdown" {
			t.Fatalf("expected stage lockdown, got %v", p.Incubation.Stage)
		}
		if p.Turning.Status != "lockdown" {
			t.Fatalf("expected turning.status lockdown, got %q", p.Turning.Status)
		}
		if p.Turning.NextTurningAt != nil {
			t.Fatalf("expected next_turning_at nil during lockdown, got %v", *p.Turning.NextTurningAt)
		}
	})

	t.Run("day 18 remains turning", func(t *testing.T) {
		p := models.DeviceTelemetryPayload{
			Incubation: models.IncubationInfo{
				Status:     "running",
				CurrentDay: intPtr(18),
				TotalDays:  intPtr(21),
				Stage:      strPtr("turning"),
			},
			Turning: models.TurningState{
				Status:        "idle",
				NextTurningAt: strPtr("2026-11-20T12:00:00Z"),
			},
		}
		normalizeIncubationStage(&p)

		if p.Incubation.Stage == nil || *p.Incubation.Stage != "turning" {
			t.Fatalf("expected stage turning on day 18, got %v", p.Incubation.Stage)
		}
		if p.Turning.Status != "idle" {
			t.Fatalf("expected turning.status idle on day 18, got %q", p.Turning.Status)
		}
		if p.Turning.NextTurningAt == nil {
			t.Fatalf("expected next_turning_at preserved on day 18")
		}
	})

	t.Run("idle clears incubation metadata and resets lockdown turning status", func(t *testing.T) {
		p := models.DeviceTelemetryPayload{
			Incubation: models.IncubationInfo{
				Status:     "idle",
				CurrentDay: intPtr(0),
				TotalDays:  intPtr(21),
				Stage:      strPtr(""),
			},
			Turning: models.TurningState{
				Status:        "lockdown",
				NextTurningAt: strPtr("2026-11-20T12:00:00Z"),
			},
		}
		normalizeIncubationStage(&p)

		if p.Incubation.CurrentDay != nil || p.Incubation.TotalDays != nil || p.Incubation.Stage != nil {
			t.Fatalf("expected idle incubation fields to be nil: %+v", p.Incubation)
		}
		if p.Turning.Status != "idle" || p.Turning.NextTurningAt != nil {
			t.Fatalf("expected idle turning fields reset, got %+v", p.Turning)
		}
	})
}
