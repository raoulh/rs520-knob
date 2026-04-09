package notify

import (
	"encoding/json"
	"io"
	"log"
	"net/http"

	"github.com/raoulh/rs520-knob/bridge/internal/rs520"
	"github.com/raoulh/rs520-knob/bridge/internal/state"
	"github.com/raoulh/rs520-knob/bridge/internal/ws"
)

// Listener receives push notifications from the RS520 on port 9284.
type Listener struct {
	cache     *state.Cache
	hub       *ws.Hub
	pollState func()
	server    *http.Server
}

// NewListener creates a notification listener.
func NewListener(addr string, cache *state.Cache, hub *ws.Hub, pollState func()) *Listener {
	l := &Listener{
		cache:     cache,
		hub:       hub,
		pollState: pollState,
	}

	mux := http.NewServeMux()
	mux.HandleFunc("POST /device_state_noti", l.handleNotification)
	mux.HandleFunc("POST /test", l.handleTest)

	l.server = &http.Server{
		Addr:    addr,
		Handler: mux,
	}
	return l
}

// ListenAndServe starts the notification HTTP server.
func (l *Listener) ListenAndServe() error {
	log.Printf("[notify] listening on %s", l.server.Addr)
	return l.server.ListenAndServe()
}

// Close shuts down the server.
func (l *Listener) Close() error {
	return l.server.Close()
}

// Handler returns the HTTP handler for testing.
func (l *Listener) Handler() http.Handler {
	return l.server.Handler
}

func (l *Listener) handleNotification(w http.ResponseWriter, r *http.Request) {
	body, err := io.ReadAll(r.Body)
	if err != nil {
		http.Error(w, "read error", http.StatusBadRequest)
		return
	}
	defer r.Body.Close()

	var msg rs520.NotificationMessage
	if err := json.Unmarshal(body, &msg); err != nil {
		log.Printf("[notify] invalid JSON: %v", err)
		http.Error(w, "invalid json", http.StatusBadRequest)
		return
	}

	log.Printf("[notify] received: %s", msg.MessageType)

	switch msg.MessageType {
	case "volume_change":
		changes := l.cache.SetVolume(msg.Volume)
		if len(changes) > 0 {
			l.hub.Broadcast(ws.NewVolumeEvent(msg.Volume))
		}

	case "mute_state_change_noti":
		mute := msg.Position != 0
		changes := l.cache.SetMute(mute)
		if len(changes) > 0 {
			s := l.cache.Snapshot()
			l.hub.Broadcast(ws.NewStateEvent(s.Volume, s.Mute, s.Playing, s.Title, s.Artist, s.Album, s.Source, s.PowerOn))
		}

	case "music_start":
		l.pollState()

	case "play_state_change", "out_put_change", "current_play_state_refresh":
		// Playback or source changed — re-poll full state
		l.pollState()

	case "queue_play", "queue_recent_track_add":
		// Queue updates — re-poll to catch new track info
		l.pollState()

	case "state_check", "abnormal_popup_close_noti", "shazam_result_noti":
		// Heartbeat / UI dismissals / informational — no action needed

	default:
		log.Printf("[notify] unknown messageType: %s", msg.MessageType)
	}

	w.WriteHeader(http.StatusOK)
}

func (l *Listener) handleTest(w http.ResponseWriter, r *http.Request) {
	w.WriteHeader(http.StatusOK)
}
