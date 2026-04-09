package notify

import (
	"bytes"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"github.com/raoulh/rs520-knob/bridge/internal/rs520"
	"github.com/raoulh/rs520-knob/bridge/internal/state"
	"github.com/raoulh/rs520-knob/bridge/internal/ws"
)

func setupListener(t *testing.T) (*Listener, *state.Cache, *ws.Hub, *int) {
	t.Helper()
	cache := state.NewCache()
	hub := ws.NewHub()
	go hub.Run()

	pollCount := 0
	pollFn := func() { pollCount++ }

	l := NewListener(":0", cache, hub, pollFn)
	return l, cache, hub, &pollCount
}

func postNotification(t *testing.T, handler http.Handler, msg rs520.NotificationMessage) *httptest.ResponseRecorder {
	t.Helper()
	body, _ := json.Marshal(msg)
	req := httptest.NewRequest(http.MethodPost, "/device_state_noti", bytes.NewReader(body))
	req.Header.Set("Content-Type", "application/json")
	w := httptest.NewRecorder()
	handler.ServeHTTP(w, req)
	return w
}

func TestVolumeChangeNotification(t *testing.T) {
	l, cache, _, _ := setupListener(t)
	handler := l.Handler()

	cache.Update(state.State{Volume: 20})

	w := postNotification(t, handler, rs520.NotificationMessage{
		MessageType: "volume_change",
		Volume:      42,
	})

	if w.Code != http.StatusOK {
		t.Errorf("expected 200, got %d", w.Code)
	}

	// Give hub goroutine time to process
	time.Sleep(10 * time.Millisecond)

	s := cache.Snapshot()
	if s.Volume != 42 {
		t.Errorf("expected volume 42, got %d", s.Volume)
	}
}

func TestMuteChangeNotification(t *testing.T) {
	l, cache, _, _ := setupListener(t)
	handler := l.Handler()

	w := postNotification(t, handler, rs520.NotificationMessage{
		MessageType: "mute_state_change_noti",
		Position:    1,
	})

	if w.Code != http.StatusOK {
		t.Errorf("expected 200, got %d", w.Code)
	}

	time.Sleep(10 * time.Millisecond)

	s := cache.Snapshot()
	if !s.Mute {
		t.Error("expected mute=true after position=1")
	}
}

func TestMusicStartTriggersPoll(t *testing.T) {
	l, _, _, pollCount := setupListener(t)
	handler := l.Handler()

	postNotification(t, handler, rs520.NotificationMessage{
		MessageType: "music_start",
	})

	if *pollCount != 1 {
		t.Errorf("expected 1 poll, got %d", *pollCount)
	}
}

func TestStateCheckHeartbeat(t *testing.T) {
	l, cache, _, pollCount := setupListener(t)
	handler := l.Handler()

	cache.Update(state.State{Volume: 10})
	postNotification(t, handler, rs520.NotificationMessage{
		MessageType: "state_check",
	})

	if *pollCount != 0 {
		t.Error("state_check should not trigger poll")
	}
	if cache.Snapshot().Volume != 10 {
		t.Error("state_check should not modify state")
	}
}

func TestHealthCheck(t *testing.T) {
	l, _, _, _ := setupListener(t)
	handler := l.Handler()

	req := httptest.NewRequest(http.MethodPost, "/test", nil)
	w := httptest.NewRecorder()
	handler.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Errorf("expected 200, got %d", w.Code)
	}
}

func TestInvalidJSON(t *testing.T) {
	l, _, _, _ := setupListener(t)
	handler := l.Handler()

	req := httptest.NewRequest(http.MethodPost, "/device_state_noti", bytes.NewReader([]byte("not json")))
	w := httptest.NewRecorder()
	handler.ServeHTTP(w, req)

	if w.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", w.Code)
	}
}
