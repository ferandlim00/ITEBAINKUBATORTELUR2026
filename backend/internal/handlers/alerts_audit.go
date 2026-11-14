package handlers

import (
	"database/sql"
	"strconv"
	"strings"
	"time"

	"backend/internal/config"
	"backend/internal/models"

	"github.com/gofiber/fiber/v3"
)

// AlertsAuditHandler serves alerts and server-side filtered audit logs.
type AlertsAuditHandler struct {
	db  *sql.DB
	cfg *config.Config
}

// NewAlertsAuditHandler creates a new AlertsAuditHandler.
func NewAlertsAuditHandler(db *sql.DB, cfg *config.Config) *AlertsAuditHandler {
	return &AlertsAuditHandler{db: db, cfg: cfg}
}

// GetAlerts handles GET /api/alerts.
func (h *AlertsAuditHandler) GetAlerts(c fiber.Ctx) error {
	rows, err := h.db.Query(
		`SELECT id, type, severity, message, created_at, status
		 FROM alerts
		 WHERE device_id = ?
		 ORDER BY FIELD(status, 'active', 'resolved'), created_at DESC, id DESC
		 LIMIT 200`,
		h.cfg.DeviceID,
	)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal mengambil data peringatan.",
		})
	}
	defer rows.Close()

	list := make([]models.AlertItem, 0)
	for rows.Next() {
		var item models.AlertItem
		var createdAt time.Time
		if err := rows.Scan(&item.ID, &item.Type, &item.Severity, &item.Message, &createdAt, &item.Status); err != nil {
			return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
				"message": "Gagal membaca data peringatan.",
			})
		}
		item.Timestamp = createdAt.UTC().Format(time.RFC3339)
		list = append(list, item)
	}

	return c.JSON(list)
}

// GetAuditLogs handles GET /api/audit-logs?from=&to=&event=&page=1&limit=50.
func (h *AlertsAuditHandler) GetAuditLogs(c fiber.Ctx) error {
	fromRaw := strings.TrimSpace(c.Query("from"))
	toRaw := strings.TrimSpace(c.Query("to"))
	eventFilter := strings.TrimSpace(c.Query("event"))

	page := 1
	if p, err := strconv.Atoi(c.Query("page", "1")); err == nil && p >= 1 {
		page = p
	}
	limit := 50
	if l, err := strconv.Atoi(c.Query("limit", "50")); err == nil && l >= 1 {
		if l > 200 {
			l = 200
		}
		limit = l
	}
	offset := (page - 1) * limit

	clauses := []string{"1=1"}
	args := make([]any, 0, 5)

	if fromRaw != "" {
		t, err := parseFlexibleISO(fromRaw)
		if err != nil {
			return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
				"message": "Parameter waktu 'from' harus berformat ISO 8601 UTC.",
			})
		}
		clauses = append(clauses, "created_at >= ?")
		args = append(args, t.UTC())
	}

	if toRaw != "" {
		t, err := parseFlexibleISO(toRaw)
		if err != nil {
			return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
				"message": "Parameter waktu 'to' harus berformat ISO 8601 UTC.",
			})
		}
		clauses = append(clauses, "created_at <= ?")
		args = append(args, t.UTC())
	}

	if eventFilter != "" {
		switch eventFilter {
		case "login", "command", "access_denied", "config_change":
			clauses = append(clauses, "event = ?")
			args = append(args, eventFilter)
		default:
			return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
				"message": "Jenis kejadian (event) tidak valid.",
			})
		}
	}

	query := `SELECT id, username, event, detail, created_at
		FROM audit_logs
		WHERE ` + strings.Join(clauses, " AND ") + `
		ORDER BY created_at DESC, id DESC
		LIMIT ? OFFSET ?`
	args = append(args, limit, offset)

	rows, err := h.db.Query(query, args...)
	if err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"message": "Gagal mengambil riwayat aktivitas.",
		})
	}
	defer rows.Close()

	logs := make([]models.AuditLogItem, 0)
	for rows.Next() {
		var item models.AuditLogItem
		var createdAt time.Time
		if err := rows.Scan(&item.ID, &item.User, &item.Event, &item.Detail, &createdAt); err != nil {
			return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
				"message": "Gagal membaca riwayat aktivitas.",
			})
		}
		item.Timestamp = createdAt.UTC().Format(time.RFC3339)
		logs = append(logs, item)
	}

	return c.JSON(logs)
}

func parseFlexibleISO(val string) (time.Time, error) {
	if t, err := time.Parse(time.RFC3339Nano, val); err == nil {
		return t, nil
	}
	return time.Parse(time.RFC3339, val)
}
