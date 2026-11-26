package services

import (
	"bytes"
	"crypto"
	"crypto/rand"
	"crypto/rsa"
	"crypto/sha256"
	"crypto/x509"
	"encoding/base64"
	"encoding/json"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"os"
	"strings"
	"sync"
	"time"
)

const DefaultFCMTopic = "incubator_alerts"

type serviceAccountKey struct {
	ProjectID   string `json:"project_id"`
	ClientEmail string `json:"client_email"`
	PrivateKey  string `json:"private_key"`
	TokenURI    string `json:"token_uri"`
}

// FCMNotifier sends push notifications via Firebase Cloud Messaging HTTP v1 API.
type FCMNotifier struct {
	projectID   string
	clientEmail string
	privateKey  *rsa.PrivateKey
	tokenURI    string
	topic       string
	client      *http.Client

	mu          sync.Mutex
	accessToken string
	tokenExpiry time.Time
}

// NewFCMNotifier loads a Firebase service account JSON file if available.
func NewFCMNotifier(credentialsPath, topic string) *FCMNotifier {
	path := strings.TrimSpace(credentialsPath)
	if path == "" {
		return nil
	}
	raw, err := os.ReadFile(path)
	if err != nil {
		if !os.IsNotExist(err) {
			log.Printf("[FCM] Gagal membaca file kredensial %s: %v", path, err)
		}
		return nil
	}

	var sa serviceAccountKey
	if err := json.Unmarshal(raw, &sa); err != nil {
		log.Printf("[FCM] Format JSON service account tidak valid: %v", err)
		return nil
	}
	if sa.ProjectID == "" || sa.ClientEmail == "" || sa.PrivateKey == "" {
		log.Printf("[FCM] File service account tidak lengkap (project_id/client_email/private_key kosong)")
		return nil
	}

	privKey, err := parseRSAPrivateKey(sa.PrivateKey)
	if err != nil {
		log.Printf("[FCM] Gagal mem-parsing RSA private key: %v", err)
		return nil
	}

	tokenURI := strings.TrimSpace(sa.TokenURI)
	if tokenURI == "" {
		tokenURI = "https://oauth2.googleapis.com/token"
	}
	targetTopic := strings.TrimSpace(topic)
	if targetTopic == "" {
		targetTopic = DefaultFCMTopic
	}

	log.Printf("[FCM] Firebase Cloud Messaging siap (project=%s, topic=%s)", sa.ProjectID, targetTopic)
	return &FCMNotifier{
		projectID:   sa.ProjectID,
		clientEmail: sa.ClientEmail,
		privateKey:  privKey,
		tokenURI:    tokenURI,
		topic:       targetTopic,
		client:      &http.Client{Timeout: 8 * time.Second},
	}
}

func parseRSAPrivateKey(pemStr string) (*rsa.PrivateKey, error) {
	block, _ := pem.Decode([]byte(pemStr))
	if block == nil {
		return nil, errors.New("blok PEM tidak ditemukan")
	}
	if key, err := x509.ParsePKCS8PrivateKey(block.Bytes); err == nil {
		if rsaKey, ok := key.(*rsa.PrivateKey); ok {
			return rsaKey, nil
		}
		return nil, errors.New("kunci PKCS8 bukan tipe RSA")
	}
	return x509.ParsePKCS1PrivateKey(block.Bytes)
}

// IsConfigured reports whether FCM credentials were loaded successfully.
func (f *FCMNotifier) IsConfigured() bool {
	return f != nil && f.privateKey != nil && f.projectID != ""
}

func (f *FCMNotifier) getAccessToken() (string, error) {
	f.mu.Lock()
	defer f.mu.Unlock()

	now := time.Now()
	if f.accessToken != "" && now.Before(f.tokenExpiry) {
		return f.accessToken, nil
	}

	headerJSON, _ := json.Marshal(map[string]string{
		"alg": "RS256",
		"typ": "JWT",
	})
	claimsJSON, _ := json.Marshal(map[string]any{
		"iss":   f.clientEmail,
		"scope": "https://www.googleapis.com/auth/firebase.messaging",
		"aud":   f.tokenURI,
		"iat":   now.Unix(),
		"exp":   now.Add(55 * time.Minute).Unix(),
	})

	signingInput := base64.RawURLEncoding.EncodeToString(headerJSON) + "." +
		base64.RawURLEncoding.EncodeToString(claimsJSON)
	digest := sha256.Sum256([]byte(signingInput))
	sig, err := rsa.SignPKCS1v15(rand.Reader, f.privateKey, crypto.SHA256, digest[:])
	if err != nil {
		return "", fmt.Errorf("gagal menandatangani JWT OAuth2: %w", err)
	}
	assertion := signingInput + "." + base64.RawURLEncoding.EncodeToString(sig)

	form := url.Values{}
	form.Set("grant_type", "urn:ietf:params:oauth:grant-type:jwt-bearer")
	form.Set("assertion", assertion)

	resp, err := f.client.Post(f.tokenURI, "application/x-www-form-urlencoded", strings.NewReader(form.Encode()))
	if err != nil {
		return "", fmt.Errorf("gagal meminta token OAuth2 Google: %w", err)
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return "", fmt.Errorf("OAuth2 Google menolak permintaan (HTTP %d): %s", resp.StatusCode, strings.TrimSpace(string(body)))
	}

	var tokResp struct {
		AccessToken string `json:"access_token"`
		ExpiresIn   int    `json:"expires_in"`
	}
	if err := json.Unmarshal(body, &tokResp); err != nil || tokResp.AccessToken == "" {
		return "", errors.New("respons access_token OAuth2 Google tidak valid")
	}

	ttl := tokResp.ExpiresIn - 120
	if ttl < 60 {
		ttl = 60
	}
	f.accessToken = tokResp.AccessToken
	f.tokenExpiry = now.Add(time.Duration(ttl) * time.Second)
	return f.accessToken, nil
}

// SendTopicAlert sends a high-priority push notification to the Android topic.
func (f *FCMNotifier) SendTopicAlert(alertType, severity, message string) error {
	if !f.IsConfigured() {
		return errors.New("Firebase Cloud Messaging belum dikonfigurasi di server")
	}

	token, err := f.getAccessToken()
	if err != nil {
		return err
	}

	rawHeader, _ := formatAlertHeaderAndLabel(alertType, severity, message)
	cleanTitle := strings.ReplaceAll(strings.ReplaceAll(rawHeader, "<b>", ""), "</b>", "")
	cleanBody := strings.TrimSpace(message)

	payload := map[string]any{
		"message": map[string]any{
			"topic": f.topic,
			"notification": map[string]string{
				"title": cleanTitle,
				"body":  cleanBody,
			},
			"data": map[string]string{
				"title":      cleanTitle,
				"body":       cleanBody,
				"alert_type": alertType,
				"severity":   severity,
			},
			"android": map[string]any{
				"priority": "HIGH",
				"notification": map[string]any{
					"channel_id":              "smart_hatchery_alerts",
					"sound":                   "default",
					"default_vibrate_timings": true,
				},
			},
		},
	}

	jsonBytes, err := json.Marshal(payload)
	if err != nil {
		return err
	}

	endpoint := fmt.Sprintf("https://fcm.googleapis.com/v1/projects/%s/messages:send", f.projectID)
	req, err := http.NewRequest(http.MethodPost, endpoint, bytes.NewReader(jsonBytes))
	if err != nil {
		return err
	}
	req.Header.Set("Authorization", "Bearer "+token)
	req.Header.Set("Content-Type", "application/json")

	resp, err := f.client.Do(req)
	if err != nil {
		return fmt.Errorf("koneksi ke FCM API gagal: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		raw, _ := io.ReadAll(io.LimitReader(resp.Body, 512))
		return fmt.Errorf("FCM API menolak permintaan (HTTP %d): %s", resp.StatusCode, strings.TrimSpace(string(raw)))
	}
	return nil
}

// SendTest sends a synchronous test push notification to subscribed Android devices.
func (f *FCMNotifier) SendTest(username string) error {
	who := strings.TrimSpace(username)
	if who == "" {
		who = "operator"
	}
	msg := fmt.Sprintf("Notifikasi Push Firebase Smart Hatchery berhasil diuji oleh %s!", who)
	return f.SendTopicAlert("general", "info", msg)
}
