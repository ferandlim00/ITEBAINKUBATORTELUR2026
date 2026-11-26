package handlers

import (
	"database/sql"
	"encoding/json"
	"fmt"

	"backend/internal/config"
	"backend/internal/database"
	"backend/internal/middleware"
	"backend/internal/models"

	"github.com/gofiber/fiber/v3"
)

// CommandsHandler validates and enqueues control commands for ESP32.
type CommandsHandler struct {
	db  *sql.DB
	cfg *config.Config
}

// NewCommandsHandler creates a new CommandsHandler.
func NewCommandsHandler(db *sql.DB, cfg *config.Config) *CommandsHandler {
	return &CommandsHandler{db: db, cfg: cfg}
}

// CreateCommand handles POST /api/commands per Kontrak API v2.1 section 4.4.
func (h *CommandsHandler) CreateCommand(c fiber.Ctx) error {
	user := middleware.CurrentUser(c)
	snap, isStale, err := GetDeviceSnapshot(h.db, h.cfg)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal memeriksa status perangkat.",
		})
	}

	if isStale || snap.Connection == "offline" {
		return c.Status(fiber.StatusConflict).JSON(fiber.Map{
			"message": "Perangkat sedang offline atau data kedaluwarsa (melewati 15 detik). Perintah ditolak.",
		})
	}

	var req models.CommandRequest
	if err := c.Bind().JSON(&req); err != nil {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"message": "Format perintah JSON tidak valid.",
		})
	}

	statusCode, actionName, auditDetail, errMsg := validateCommand(snap, req)
	if errMsg != "" {
		return c.Status(statusCode).JSON(fiber.Map{
			"message": errMsg,
		})
	}

	payloadBytes, _ := json.Marshal(req)
	res, err := h.db.Exec(
		`INSERT INTO commands (device_id, user_id, target, action, payload_json, status, created_at, updated_at)
		 VALUES (?, ?, ?, ?, ?, 'dispatched', UTC_TIMESTAMP(), UTC_TIMESTAMP())`,
		h.cfg.DeviceID, user.ID, req.Target, actionName, string(payloadBytes),
	)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal menyimpan perintah ke antrean perangkat.",
		})
	}

	cmdID, _ := res.LastInsertId()
	database.WriteAuditLog(h.db, user.Username, "command", auditDetail)

	return c.Status(fiber.StatusAccepted).JSON(fiber.Map{
		"id":             cmdID,
		"status":         "sent",
		"delivery_state": "dispatched",
	})
}

func isIncubationLockdown(inc models.IncubationInfo, turning models.TurningState) bool {
	if inc.Stage != nil && *inc.Stage == "lockdown" {
		return true
	}
	if turning.Status == "lockdown" {
		return true
	}
	return inc.CurrentDay != nil && *inc.CurrentDay >= 19
}

func validateCommand(snap models.TelemetryResponse, req models.CommandRequest) (int, string, string, string) {
	switch req.Target {
	case "incubation":
		if req.Action == "start" {
			if snap.Incubation.Status == "running" {
				return fiber.StatusConflict, "", "", "Siklus inkubasi sudah sedang berjalan."
			}
			return 0, "start", "Siklus inkubasi 21 hari dimulai", ""
		}
		if req.Action == "stop" {
			if snap.Incubation.Status == "idle" {
				return fiber.StatusConflict, "", "", "Siklus inkubasi belum berjalan."
			}
			return 0, "stop", "Siklus inkubasi dihentikan", ""
		}
		return fiber.StatusBadRequest, "", "", "Aksi inkubasi tidak dikenali (gunakan 'start' atau 'stop')."

	case "lamp":
		if req.Mode == "auto" {
			return 0, "auto", "Lampu diubah ke mode otomatis (Fuzzy)", ""
		}
		if req.Mode == "manual" {
			if req.Intensity == nil || *req.Intensity < 0 || *req.Intensity > 80 {
				return fiber.StatusBadRequest, "", "", "Intensitas lampu manual wajib di antara 0% sampai 80%."
			}
			return 0, "manual", fmt.Sprintf("Lampu diubah ke mode manual (%d%%)", *req.Intensity), ""
		}
		return fiber.StatusBadRequest, "", "", "Mode lampu tidak valid (gunakan 'auto' atau 'manual')."

	case "ventilation":
		if req.Mode == "auto" {
			return 0, "auto", "Ventilasi diubah ke mode otomatis (Fuzzy)", ""
		}
		if req.Mode == "manual" {
			if req.Angle == nil || *req.Angle < 0 || *req.Angle > 90 {
				return fiber.StatusBadRequest, "", "", "Sudut bukaan ventilasi manual wajib di antara 0° sampai 90°."
			}
			return 0, "manual", fmt.Sprintf("Ventilasi diubah ke mode manual (%d°)", *req.Angle), ""
		}
		return fiber.StatusBadRequest, "", "", "Mode ventilasi tidak valid (gunakan 'auto' atau 'manual')."

	case "turning":
		if req.Action != "turn_now" {
			return fiber.StatusBadRequest, "", "", "Aksi pembalikan tidak dikenali (gunakan 'turn_now')."
		}
		if snap.Incubation.Status != "running" {
			return fiber.StatusConflict, "", "", "Pembalikan ditolak karena siklus inkubasi belum berjalan."
		}
		if isIncubationLockdown(snap.Incubation, snap.Turning) {
			return fiber.StatusConflict, "", "", "Pembalikan ditolak karena inkubasi berada pada fase lockdown (Hari 19-21)."
		}
		if snap.Turning.Status == "fault" || !snap.Motor.EncoderOK {
			return fiber.StatusConflict, "", "", "Pembalikan ditolak karena motor pembalik sedang mengalami gangguan."
		}
		if snap.Motor.Running || snap.Turning.Status == "turning" {
			return fiber.StatusConflict, "", "", "Pembalikan ditolak karena motor sedang berputar."
		}
		return 0, "turn_now", "Pembalikan telur manual dipicu (turn_now)", ""

	default:
		return fiber.StatusBadRequest, "", "", "Target perintah tidak dikenali."
	}
}
