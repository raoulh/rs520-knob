package ws

import (
	"encoding/json"
	"log"
	"net/http"
	"time"

	"github.com/gorilla/websocket"
	"github.com/raoulh/rs520-knob/bridge/internal/rs520"
	"github.com/raoulh/rs520-knob/bridge/internal/state"
)

const (
	writeWait  = 10 * time.Second
	pongWait   = 60 * time.Second
	pingPeriod = 30 * time.Second
)

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin:     func(r *http.Request) bool { return true }, // local network only
}

// Handler handles WebSocket connections from the knob.
type Handler struct {
	hub       *Hub
	rs520     *rs520.Client
	cache     *state.Cache
	pollState func() // callback to trigger full state poll
}

// NewHandler creates a WebSocket handler.
func NewHandler(hub *Hub, rs520Client *rs520.Client, cache *state.Cache, pollState func()) *Handler {
	return &Handler{
		hub:       hub,
		rs520:     rs520Client,
		cache:     cache,
		pollState: pollState,
	}
}

// ServeHTTP upgrades the HTTP connection to WebSocket and starts pumps.
func (h *Handler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	conn, err := upgrader.Upgrade(w, r, nil)
	if err != nil {
		log.Printf("[ws] upgrade error: %v", err)
		return
	}

	client := NewClient(h.hub, conn)
	h.hub.Register(client)

	// Send initial state snapshot
	s := h.cache.Snapshot()
	evt := NewStateEvent(s.Volume, s.Mute, s.Playing, s.Title, s.Artist, s.Album, s.Source, s.PowerOn)
	if data, err := json.Marshal(evt); err == nil {
		client.send <- data
	}

	// Send connected event
	connEvt := NewConnectedEvent("RS520")
	if data, err := json.Marshal(connEvt); err == nil {
		client.send <- data
	}

	go client.WritePump()
	go h.readPump(client)
}

func (h *Handler) readPump(client *Client) {
	defer func() {
		h.hub.Unregister(client)
		client.conn.Close()
	}()

	for {
		_, message, err := client.conn.ReadMessage()
		if err != nil {
			if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseNormalClosure) {
				log.Printf("[ws] read error: %v", err)
			}
			return
		}

		cmd, err := ParseCommand(message)
		if err != nil {
			log.Printf("[ws] invalid command: %v", err)
			errEvt := NewErrorEvent(err.Error())
			if data, merr := json.Marshal(errEvt); merr == nil {
				client.send <- data
			}
			continue
		}

		h.handleCommand(cmd)
	}
}

func (h *Handler) handleCommand(cmd *Command) {
	switch cmd.Cmd {
	case CmdVolume:
		vol, err := VolumeFromCommand(cmd)
		if err != nil {
			log.Printf("[ws] volume error: %v", err)
			return
		}
		resp, err := h.rs520.SetVolume(vol)
		if err != nil {
			log.Printf("[ws] set volume error: %v", err)
			return
		}
		changes := h.cache.SetVolume(resp.VolumeValue)
		if len(changes) > 0 {
			h.hub.Broadcast(NewVolumeEvent(resp.VolumeValue))
		}

	case CmdPlayPause:
		if err := h.rs520.PlayPause(); err != nil {
			log.Printf("[ws] play_pause error: %v", err)
			return
		}
		h.pollState()

	case CmdNext:
		if err := h.rs520.Next(); err != nil {
			log.Printf("[ws] next error: %v", err)
			return
		}
		h.pollState()

	case CmdPrev:
		if err := h.rs520.Prev(); err != nil {
			log.Printf("[ws] prev error: %v", err)
			return
		}
		h.pollState()

	case CmdMute:
		if err := h.rs520.ToggleMute(); err != nil {
			log.Printf("[ws] mute error: %v", err)
			return
		}
		// Poll mute state after toggle
		muteResp, err := h.rs520.GetMuteState()
		if err != nil {
			log.Printf("[ws] get mute state error: %v", err)
			return
		}
		changes := h.cache.SetMute(muteResp.Mute)
		if len(changes) > 0 {
			s := h.cache.Snapshot()
			h.hub.Broadcast(NewStateEvent(s.Volume, s.Mute, s.Playing, s.Title, s.Artist, s.Album, s.Source, s.PowerOn))
		}

	case CmdPower:
		if err := h.rs520.TogglePower(); err != nil {
			log.Printf("[ws] power error: %v", err)
			return
		}
	}
}
