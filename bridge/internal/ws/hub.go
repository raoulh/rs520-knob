package ws

import (
	"encoding/json"
	"log"
	"sync"
)

// Hub manages connected WebSocket clients and broadcasts events.
type Hub struct {
	mu      sync.RWMutex
	clients map[*Client]bool

	register   chan *Client
	unregister chan *Client
	broadcast  chan *Event
}

// Client represents a connected WebSocket client.
type Client struct {
	hub  *Hub
	conn WSConn
	send chan []byte
}

// WSConn abstracts a WebSocket connection for testability.
type WSConn interface {
	WriteMessage(messageType int, data []byte) error
	ReadMessage() (messageType int, p []byte, err error)
	Close() error
}

// NewHub creates a new hub.
func NewHub() *Hub {
	return &Hub{
		clients:    make(map[*Client]bool),
		register:   make(chan *Client),
		unregister: make(chan *Client),
		broadcast:  make(chan *Event, 256),
	}
}

// Run starts the hub event loop. Call in a goroutine.
func (h *Hub) Run() {
	for {
		select {
		case client := <-h.register:
			h.mu.Lock()
			h.clients[client] = true
			h.mu.Unlock()
			log.Printf("[hub] client connected (%d total)", h.ClientCount())

		case client := <-h.unregister:
			h.mu.Lock()
			if _, ok := h.clients[client]; ok {
				delete(h.clients, client)
				close(client.send)
			}
			h.mu.Unlock()
			log.Printf("[hub] client disconnected (%d remaining)", h.ClientCount())

		case event := <-h.broadcast:
			data, err := json.Marshal(event)
			if err != nil {
				log.Printf("[hub] marshal error: %v", err)
				continue
			}
			h.mu.RLock()
			for client := range h.clients {
				select {
				case client.send <- data:
				default:
					// Client send buffer full — disconnect
					go func(c *Client) {
						h.unregister <- c
					}(client)
				}
			}
			h.mu.RUnlock()
		}
	}
}

// Broadcast sends an event to all connected clients.
func (h *Hub) Broadcast(event *Event) {
	h.broadcast <- event
}

// Register adds a client to the hub.
func (h *Hub) Register(client *Client) {
	h.register <- client
}

// Unregister removes a client from the hub.
func (h *Hub) Unregister(client *Client) {
	h.unregister <- client
}

// ClientCount returns the number of connected clients.
func (h *Hub) ClientCount() int {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return len(h.clients)
}

// NewClient creates a new client for the hub.
func NewClient(hub *Hub, conn WSConn) *Client {
	return &Client{
		hub:  hub,
		conn: conn,
		send: make(chan []byte, 256),
	}
}

// SendJSON sends a JSON-encoded event directly to this client.
func (c *Client) SendJSON(event *Event) error {
	data, err := json.Marshal(event)
	if err != nil {
		return err
	}
	c.send <- data
	return nil
}

// WritePump pumps messages from the send channel to the WebSocket connection.
func (c *Client) WritePump() {
	defer c.conn.Close()
	for data := range c.send {
		if err := c.conn.WriteMessage(1, data); err != nil { // 1 = TextMessage
			return
		}
	}
}
