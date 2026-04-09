package rs520

import (
	"crypto/tls"
	"encoding/json"
	"io"
	"net/http"
	"net/http/httptest"
	"testing"
)

func newTestServer(t *testing.T, handler http.Handler) (*Client, *httptest.Server) {
	t.Helper()
	srv := httptest.NewTLSServer(handler)
	client := NewClientWithHTTP(srv.URL, srv.Client())
	return client, srv
}

func readBody(t *testing.T, r *http.Request) []byte {
	t.Helper()
	data, err := io.ReadAll(r.Body)
	if err != nil {
		t.Fatalf("read body: %v", err)
	}
	return data
}

func TestDeviceName(t *testing.T) {
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/device_name" {
			t.Errorf("unexpected path: %s", r.URL.Path)
		}
		if r.Method != http.MethodPost {
			t.Errorf("expected POST, got %s", r.Method)
		}
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(`{"data":{"name":"RoseSalon"}}`))
	}))
	defer srv.Close()

	resp, err := c.DeviceName()
	if err != nil {
		t.Fatalf("DeviceName: %v", err)
	}
	if resp == nil {
		t.Fatal("expected response, got nil")
	}
}

func TestDeviceConnected(t *testing.T) {
	var gotIP string
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body := readBody(t, r)
		var req DeviceConnectedRequest
		json.Unmarshal(body, &req)
		gotIP = req.ConnectIP
		w.Write([]byte(`{"menuArr":[]}`))
	}))
	defer srv.Close()

	err := c.DeviceConnected("192.168.30.100")
	if err != nil {
		t.Fatalf("DeviceConnected: %v", err)
	}
	if gotIP != "192.168.30.100" {
		t.Errorf("expected IP 192.168.30.100, got %q", gotIP)
	}
}

func TestSetVolume(t *testing.T) {
	var gotVol int
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body := readBody(t, r)
		var req VolumeRequest
		json.Unmarshal(body, &req)
		gotVol = req.Volume
		w.Header().Set("Content-Type", "application/json")
		json.NewEncoder(w).Encode(VolumeResponse{VolumeValue: req.Volume})
	}))
	defer srv.Close()

	resp, err := c.SetVolume(25)
	if err != nil {
		t.Fatalf("SetVolume: %v", err)
	}
	if gotVol != 25 {
		t.Errorf("expected volume 25, got %d", gotVol)
	}
	if resp.VolumeValue != 25 {
		t.Errorf("expected response volume 25, got %d", resp.VolumeValue)
	}
}

func TestGetMuteState(t *testing.T) {
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/mute.state.get" {
			t.Errorf("unexpected path: %s", r.URL.Path)
		}
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(`{"mute":true}`))
	}))
	defer srv.Close()

	resp, err := c.GetMuteState()
	if err != nil {
		t.Fatalf("GetMuteState: %v", err)
	}
	if !resp.Mute {
		t.Error("expected mute=true")
	}
}

func TestPlayPause(t *testing.T) {
	var gotState int
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body := readBody(t, r)
		var req PlayStateRequest
		json.Unmarshal(body, &req)
		gotState = req.CurrentPlayState
		w.Write([]byte(`{}`))
	}))
	defer srv.Close()

	if err := c.PlayPause(); err != nil {
		t.Fatalf("PlayPause: %v", err)
	}
	if gotState != PlayPauseToggle {
		t.Errorf("expected state %d, got %d", PlayPauseToggle, gotState)
	}
}

func TestNext(t *testing.T) {
	var gotState int
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body := readBody(t, r)
		var req PlayStateRequest
		json.Unmarshal(body, &req)
		gotState = req.CurrentPlayState
		w.Write([]byte(`{}`))
	}))
	defer srv.Close()

	if err := c.Next(); err != nil {
		t.Fatalf("Next: %v", err)
	}
	if gotState != NextTrack {
		t.Errorf("expected state %d, got %d", NextTrack, gotState)
	}
}

func TestPrev(t *testing.T) {
	var gotState int
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body := readBody(t, r)
		var req PlayStateRequest
		json.Unmarshal(body, &req)
		gotState = req.CurrentPlayState
		w.Write([]byte(`{}`))
	}))
	defer srv.Close()

	if err := c.Prev(); err != nil {
		t.Fatalf("Prev: %v", err)
	}
	if gotState != PrevTrack {
		t.Errorf("expected state %d, got %d", PrevTrack, gotState)
	}
}

func TestToggleMute(t *testing.T) {
	var gotBar string
	var gotVal int
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body := readBody(t, r)
		var req RemoteBarRequest
		json.Unmarshal(body, &req)
		gotBar = req.BarControl
		gotVal = req.Value
		w.Write([]byte(`{}`))
	}))
	defer srv.Close()

	if err := c.ToggleMute(); err != nil {
		t.Fatalf("ToggleMute: %v", err)
	}
	if gotBar != BarMute {
		t.Errorf("expected bar %q, got %q", BarMute, gotBar)
	}
	if gotVal != -1 {
		t.Errorf("expected value -1, got %d", gotVal)
	}
}

func TestTogglePower(t *testing.T) {
	var gotBar string
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		body := readBody(t, r)
		var req RemoteBarRequest
		json.Unmarshal(body, &req)
		gotBar = req.BarControl
		w.Write([]byte(`{}`))
	}))
	defer srv.Close()

	if err := c.TogglePower(); err != nil {
		t.Fatalf("TogglePower: %v", err)
	}
	if gotBar != BarPower {
		t.Errorf("expected bar %q, got %q", BarPower, gotBar)
	}
}

func TestGetCurrentState(t *testing.T) {
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(`{"data":{"titleName":"Test Song","artistName":"Test Artist","albumName":"Test Album","albumArtId":"abc123"}}`))
	}))
	defer srv.Close()

	resp, err := c.GetCurrentState()
	if err != nil {
		t.Fatalf("GetCurrentState: %v", err)
	}
	if resp.Data.TitleName != "Test Song" {
		t.Errorf("expected title 'Test Song', got %q", resp.Data.TitleName)
	}
	if resp.Data.ArtistName != "Test Artist" {
		t.Errorf("expected artist 'Test Artist', got %q", resp.Data.ArtistName)
	}
	if resp.Data.AlbumArtID != "abc123" {
		t.Errorf("expected art ID 'abc123', got %q", resp.Data.AlbumArtID)
	}
}

func TestGetControlInfo(t *testing.T) {
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			t.Errorf("expected GET, got %s", r.Method)
		}
		w.Header().Set("Content-Type", "application/json")
		w.Write([]byte(`{"volume":42,"mute":false,"playState":1,"source":"tidal"}`))
	}))
	defer srv.Close()

	resp, err := c.GetControlInfo()
	if err != nil {
		t.Fatalf("GetControlInfo: %v", err)
	}
	if resp.Volume != 42 {
		t.Errorf("expected volume 42, got %d", resp.Volume)
	}
	if resp.Source != "tidal" {
		t.Errorf("expected source 'tidal', got %q", resp.Source)
	}
}

func TestContentTypeHeader(t *testing.T) {
	var gotCT string
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		gotCT = r.Header.Get("Content-Type")
		w.Write([]byte(`{}`))
	}))
	defer srv.Close()

	c.SetVolume(10)
	if gotCT != "application/json;charset=utf-8" {
		t.Errorf("expected Content-Type header, got %q", gotCT)
	}
}

func TestHTTPError(t *testing.T) {
	c, srv := newTestServer(t, http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.WriteHeader(http.StatusInternalServerError)
		w.Write([]byte(`server error`))
	}))
	defer srv.Close()

	_, err := c.SetVolume(10)
	if err == nil {
		t.Fatal("expected error for 500 response")
	}
}

func TestNewClientDefaultTLS(t *testing.T) {
	c := NewClient("192.168.1.100")
	if c.baseURL != "https://192.168.1.100:9283" {
		t.Errorf("unexpected baseURL: %s", c.baseURL)
	}
	if c.mediaURL != "http://192.168.1.100:8000" {
		t.Errorf("unexpected mediaURL: %s", c.mediaURL)
	}
	transport := c.httpClient.Transport.(*http.Transport)
	if !transport.TLSClientConfig.InsecureSkipVerify {
		t.Error("expected InsecureSkipVerify=true")
	}
}

// Suppress unused import warning for tls.
var _ = tls.Config{}
