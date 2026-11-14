package middleware

import (
	"sync"
	"time"
)

// LoginRateLimiter tracks failed login attempts per IP within a 1-minute sliding window.
type LoginRateLimiter struct {
	mu       sync.Mutex
	attempts map[string][]time.Time
	limit    int
	window   time.Duration
}

// NewLoginRateLimiter creates a limiter allowing up to maxFailures per window.
func NewLoginRateLimiter(maxFailures int, window time.Duration) *LoginRateLimiter {
	return &LoginRateLimiter{
		attempts: make(map[string][]time.Time),
		limit:    maxFailures,
		window:   window,
	}
}

// IsBlocked returns true when the IP has already reached maxFailures within the window.
func (l *LoginRateLimiter) IsBlocked(ip string) bool {
	l.mu.Lock()
	defer l.mu.Unlock()

	now := time.Now()
	cutoff := now.Add(-l.window)
	valid := l.prune(ip, cutoff)
	if len(valid) == 0 {
		delete(l.attempts, ip)
		return false
	}
	l.attempts[ip] = valid
	return len(valid) >= l.limit
}

// RecordFailure logs a failed login attempt for the given IP.
func (l *LoginRateLimiter) RecordFailure(ip string) {
	l.mu.Lock()
	defer l.mu.Unlock()

	now := time.Now()
	cutoff := now.Add(-l.window)
	valid := l.prune(ip, cutoff)
	valid = append(valid, now)
	l.attempts[ip] = valid
}

// Reset clears failure history for an IP after a successful login.
func (l *LoginRateLimiter) Reset(ip string) {
	l.mu.Lock()
	defer l.mu.Unlock()
	delete(l.attempts, ip)
}

func (l *LoginRateLimiter) prune(ip string, cutoff time.Time) []time.Time {
	list := l.attempts[ip]
	if len(list) == 0 {
		return nil
	}
	valid := make([]time.Time, 0, len(list))
	for _, t := range list {
		if t.After(cutoff) {
			valid = append(valid, t)
		}
	}
	return valid
}
