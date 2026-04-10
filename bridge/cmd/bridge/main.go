package main

import (
	"context"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"strconv"
	"syscall"
	"time"

	"github.com/hashicorp/mdns"
	"github.com/raoulh/rs520-knob/bridge/internal/artwork"
	"github.com/raoulh/rs520-knob/bridge/internal/notify"
	"github.com/raoulh/rs520-knob/bridge/internal/rs520"
	"github.com/raoulh/rs520-knob/bridge/internal/state"
	"github.com/raoulh/rs520-knob/bridge/internal/ws"
)

func main() {
	cfg := loadConfig()
	log.Printf("[bridge] starting — RS520=%s:%d WS=:%d Notify=:%d poll=%s",
		cfg.RS520Host, cfg.RS520Port, cfg.WSPort, cfg.NotifyPort, cfg.PollInterval)

	// RS520 HTTPS client
	client := rs520.NewClient(cfg.RS520Host)

	// Ping device
	resp, err := client.DeviceName()
	if err != nil {
		log.Printf("[bridge] WARNING: RS520 not reachable: %v", err)
	} else {
		log.Printf("[bridge] RS520 connected: %s", string(resp.Data))
	}

	// State cache
	cache := state.NewCache()

	// WS hub
	hub := ws.NewHub()
	go hub.Run()

	// Bridge IP for push notification callback
	bridgeIP := getOutboundIP()
	log.Printf("[bridge] callback IP: %s", bridgeIP)

	// State poller function — also re-registers with RS520 each cycle
	// to survive amp reboots (device_connected is idempotent)
	pollState := func() {
		pollDeviceState(client, cache, hub, bridgeIP)
	}

	// Initial state poll + registration
	pollState()

	// Notification listener (:9284)
	notifyListener := notify.NewListener(
		fmt.Sprintf(":%d", cfg.NotifyPort),
		cache, hub, pollState,
	)
	go func() {
		if err := notifyListener.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Printf("[notify] error: %v", err)
		}
	}()

	// Artwork proxy
	artProxy := artwork.NewProxy(cfg.RS520Host, nil)

	// WS handler
	wsHandler := ws.NewHandler(hub, client, cache, pollState)

	// HTTP mux
	mux := http.NewServeMux()
	mux.Handle("/ws", wsHandler)
	mux.Handle("/art/", artProxy)
	mux.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		fmt.Fprintf(w, `{"status":"ok","clients":%d}`, hub.ClientCount())
	})

	wsServer := &http.Server{
		Addr:    fmt.Sprintf(":%d", cfg.WSPort),
		Handler: mux,
	}

	// Background state poller
	ticker := time.NewTicker(cfg.PollInterval)
	go func() {
		for range ticker.C {
			pollState()
		}
	}()

	// Start WS server
	go func() {
		log.Printf("[bridge] WS server listening on :%d", cfg.WSPort)
		if err := wsServer.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			log.Fatalf("[bridge] server error: %v", err)
		}
	}()

	// mDNS service advertisement
	// Pass explicit IPs — hashicorp/mdns cannot resolve Docker hostnames
	var mdnsIPs []net.IP
	if bridgeIP != "" {
		mdnsIPs = []net.IP{net.ParseIP(bridgeIP)}
	}
	mdnsService, err := mdns.NewMDNSService(
		"RS520 Bridge",       // instance name
		"_rs520bridge._tcp",  // service type
		"",                   // domain (default .local)
		"",                   // host (auto)
		cfg.WSPort,           // port
		mdnsIPs,              // explicit IPs
		[]string{"version=0.1.0", fmt.Sprintf("rs520=%s", cfg.RS520Host)}, // TXT
	)
	if err != nil {
		log.Printf("[mdns] WARNING: service init failed: %v — mDNS disabled", err)
	}
	var mdnsServer *mdns.Server
	if mdnsService != nil {
		mdnsServer, err = mdns.NewServer(&mdns.Config{Zone: mdnsService})
		if err != nil {
			log.Printf("[mdns] WARNING: server start failed: %v — mDNS disabled", err)
		} else {
			log.Printf("[mdns] advertising _rs520bridge._tcp on port %d (IP: %s)", cfg.WSPort, bridgeIP)
		}
	}

	// Graceful shutdown
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
	sig := <-sigCh
	log.Printf("[bridge] received %s, shutting down...", sig)

	ticker.Stop()
	if mdnsServer != nil {
		mdnsServer.Shutdown()
	}

	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	notifyListener.Close()
	wsServer.Shutdown(ctx)

	log.Println("[bridge] shutdown complete")
}

type config struct {
	RS520Host    string
	RS520Port    int
	WSPort       int
	NotifyPort   int
	PollInterval time.Duration
}

func loadConfig() config {
	cfg := config{
		RS520Host:    envOrDefault("RS520_HOST", "192.168.30.135"),
		RS520Port:    envIntOrDefault("RS520_PORT", 9283),
		WSPort:       envIntOrDefault("WS_PORT", 8080),
		NotifyPort:   envIntOrDefault("NOTIFY_PORT", 9284),
		PollInterval: envDurationOrDefault("POLL_INTERVAL", 5*time.Second),
	}
	return cfg
}

func envOrDefault(key, def string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return def
}

func envIntOrDefault(key string, def int) int {
	if v := os.Getenv(key); v != "" {
		if n, err := strconv.Atoi(v); err == nil {
			return n
		}
	}
	return def
}

func envDurationOrDefault(key string, def time.Duration) time.Duration {
	if v := os.Getenv(key); v != "" {
		if d, err := time.ParseDuration(v); err == nil {
			return d
		}
	}
	return def
}

func pollDeviceState(client *rs520.Client, cache *state.Cache, hub *ws.Hub, bridgeIP string) {
	// Re-register as controller every poll cycle.
	// This is idempotent and ensures push notifications resume
	// after an RS520 reboot.
	if bridgeIP != "" {
		if err := client.DeviceConnected(bridgeIP); err != nil {
			log.Printf("[poll] device_connected error: %v", err)
			// Don't return — still try to poll state
		}
	}

	controlInfo, err := client.GetControlInfo()
	if err != nil {
		log.Printf("[poll] get_control_info error: %v", err)
		return
	}

	currentState, err := client.GetCurrentState()
	if err != nil {
		log.Printf("[poll] get_current_state error: %v", err)
		return
	}

	newState := state.State{
		Volume:     controlInfo.Volume,
		Mute:       controlInfo.Mute,
		Playing:    controlInfo.PlayState == 1,
		Title:      currentState.Data.TitleName,
		Artist:     currentState.Data.ArtistName,
		Album:      currentState.Data.AlbumName,
		AlbumArtID: currentState.Data.AlbumArtID,
		Source:     controlInfo.Source,
		PowerOn:    true,
		Position:   int(currentState.Data.CurrentPosition),
		Duration:   int(currentState.Data.Duration),
		TrackInfo:  currentState.Data.TrackInfo,
	}

	changes := cache.Update(newState)
	if len(changes) > 0 {
		s := cache.Snapshot()
		hub.Broadcast(ws.NewStateEvent(s.Volume, s.Mute, s.Playing, s.Title, s.Artist, s.Album, s.Source, s.TrackInfo, s.Position, s.Duration, s.PowerOn))

		// Check if artwork changed
		for _, ch := range changes {
			if ch.Field == "albumArtId" && s.AlbumArtID != "" {
				hub.Broadcast(ws.NewArtworkEvent(fmt.Sprintf("/art/current?id=%s&format=jpeg", s.AlbumArtID)))
			}
		}
	}

	// Always send position update (changes every poll)
	s := cache.Snapshot()
	hub.Broadcast(ws.NewPositionEvent(s.Position, s.Duration))
}

func getOutboundIP() string {
	conn, err := net.Dial("udp", "8.8.8.8:80")
	if err != nil {
		log.Printf("[bridge] cannot determine outbound IP: %v", err)
		return ""
	}
	defer conn.Close()
	localAddr := conn.LocalAddr().(*net.UDPAddr)
	return localAddr.IP.String()
}
