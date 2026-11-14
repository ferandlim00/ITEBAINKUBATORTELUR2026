package middleware

import (
	"testing"
	"time"
)

func TestLoginRateLimiter(t *testing.T) {
	lim := NewLoginRateLimiter(5, time.Minute)
	ip := "127.0.0.1"

	for i := 0; i < 5; i++ {
		if lim.IsBlocked(ip) {
			t.Fatalf("should not be blocked before 5 failures (attempt %d)", i)
		}
		lim.RecordFailure(ip)
	}

	if !lim.IsBlocked(ip) {
		t.Fatalf("expected IP to be blocked after 5 failures")
	}

	lim.Reset(ip)
	if lim.IsBlocked(ip) {
		t.Fatalf("expected IP to be unblocked after Reset")
	}
}
