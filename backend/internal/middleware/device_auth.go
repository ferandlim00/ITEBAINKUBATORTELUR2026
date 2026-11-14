package middleware

import (
	"crypto/subtle"

	"github.com/gofiber/fiber/v3"
)

// RequireDeviceKey authenticates ESP32 requests using the X-Device-Key header.
func RequireDeviceKey(expectedKey string) fiber.Handler {
	return func(c fiber.Ctx) error {
		provided := c.Get("X-Device-Key")
		if len(provided) == 0 || subtle.ConstantTimeCompare([]byte(provided), []byte(expectedKey)) != 1 {
			return c.Status(fiber.StatusUnauthorized).JSON(fiber.Map{
				"message": "Kunci perangkat tidak valid.",
			})
		}
		return c.Next()
	}
}
