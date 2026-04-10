package rs520

import (
	"bytes"
	"crypto/tls"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"log"
	"net/http"
	"strings"
	"time"
)

// Client communicates with the RS520 HTTPS API on port 9283.
type Client struct {
	baseURL    string
	mediaURL   string
	httpClient *http.Client
}

// NewClient creates an RS520 API client.
// host is the RS520 IP (e.g., "192.168.30.135").
func NewClient(host string) *Client {
	return &Client{
		baseURL:  fmt.Sprintf("https://%s:9283", host),
		mediaURL: fmt.Sprintf("http://%s:8000", host),
		httpClient: &http.Client{
			Timeout: 5 * time.Second,
			Transport: &http.Transport{
				TLSClientConfig: &tls.Config{
					InsecureSkipVerify: true, // RS520 uses self-signed cert
				},
				MaxIdleConns:        5,
				MaxIdleConnsPerHost: 2,
				IdleConnTimeout:     30 * time.Second,
				DisableKeepAlives:   true, // RS520 drops keep-alive unpredictably → fresh conn each time
			},
		},
	}
}

// NewClientWithHTTP creates a client using a custom http.Client (for testing).
func NewClientWithHTTP(baseURL string, httpClient *http.Client) *Client {
	return &Client{
		baseURL:    baseURL,
		mediaURL:   baseURL, // tests use same server
		httpClient: httpClient,
	}
}

func (c *Client) post(path string, body any) ([]byte, error) {
	var bodyData []byte
	if body != nil {
		var err error
		bodyData, err = json.Marshal(body)
		if err != nil {
			return nil, fmt.Errorf("marshal request: %w", err)
		}
	}

	// Retry once on EOF — RS520 sometimes resets the TLS connection
	for attempt := 0; attempt < 2; attempt++ {
		var reqBody io.Reader
		if bodyData != nil {
			reqBody = bytes.NewReader(bodyData)
		}

		req, err := http.NewRequest(http.MethodPost, c.baseURL+path, reqBody)
		if err != nil {
			return nil, fmt.Errorf("create request: %w", err)
		}
		req.Header.Set("Content-Type", "application/json;charset=utf-8")

		resp, err := c.httpClient.Do(req)
		if err != nil {
			if attempt == 0 && isEOF(err) {
				log.Printf("[rs520] %s EOF, retrying", path)
				continue
			}
			return nil, fmt.Errorf("request %s: %w", path, err)
		}
		defer resp.Body.Close()

		respBody, err := io.ReadAll(resp.Body)
		if err != nil {
			return nil, fmt.Errorf("read response: %w", err)
		}

		if resp.StatusCode < 200 || resp.StatusCode >= 300 {
			return nil, fmt.Errorf("request %s returned %d: %s", path, resp.StatusCode, string(respBody))
		}

		return respBody, nil
	}
	return nil, fmt.Errorf("request %s: unexpected retry fallthrough", path)
}

func (c *Client) get(path string) ([]byte, error) {
	req, err := http.NewRequest(http.MethodGet, c.baseURL+path, nil)
	if err != nil {
		return nil, fmt.Errorf("create request: %w", err)
	}

	resp, err := c.httpClient.Do(req)
	if err != nil {
		return nil, fmt.Errorf("request %s: %w", path, err)
	}
	defer resp.Body.Close()

	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, fmt.Errorf("read response: %w", err)
	}

	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return nil, fmt.Errorf("request %s returned %d: %s", path, resp.StatusCode, string(body))
	}

	return body, nil
}

// DeviceName pings the RS520 and retrieves its name.
func (c *Client) DeviceName() (*DeviceNameResponse, error) {
	data, err := c.post("/device_name", nil)
	if err != nil {
		return nil, err
	}
	var resp DeviceNameResponse
	if err := json.Unmarshal(data, &resp); err != nil {
		return nil, fmt.Errorf("decode device_name: %w", err)
	}
	return &resp, nil
}

// DeviceConnected registers the bridge as a controller.
func (c *Client) DeviceConnected(ip string) error {
	_, err := c.post("/device_connected", DeviceConnectedRequest{ConnectIP: ip})
	return err
}

// GetCurrentState returns full playback state.
func (c *Client) GetCurrentState() (*CurrentStateResponse, error) {
	data, err := c.post("/get_current_state", nil)
	if err != nil {
		return nil, err
	}
	var resp CurrentStateResponse
	if err := json.Unmarshal(data, &resp); err != nil {
		return nil, fmt.Errorf("decode current_state: %w", err)
	}
	return &resp, nil
}

// GetControlInfo returns volume, playback state, and source info.
func (c *Client) GetControlInfo() (*ControlInfoResponse, error) {
	data, err := c.get("/get_control_info")
	if err != nil {
		return nil, err
	}
	var resp ControlInfoResponse
	if err := json.Unmarshal(data, &resp); err != nil {
		return nil, fmt.Errorf("decode control_info: %w", err)
	}
	return &resp, nil
}

// SetVolume sets the RS520 volume (0-100).
func (c *Client) SetVolume(vol int) (*VolumeResponse, error) {
	data, err := c.post("/volume", VolumeRequest{VolumeType: "volume_set", VolumeValue: vol})
	if err != nil {
		return nil, err
	}
	if len(data) == 0 {
		return &VolumeResponse{VolumeValue: vol}, nil
	}
	var resp VolumeResponse
	if err := json.Unmarshal(data, &resp); err != nil {
		return &VolumeResponse{VolumeValue: vol}, nil
	}
	return &resp, nil
}

// GetMuteState returns the current mute state.
func (c *Client) GetMuteState() (*MuteResponse, error) {
	data, err := c.post("/mute.state.get", nil)
	if err != nil {
		return nil, err
	}
	var resp MuteResponse
	if err := json.Unmarshal(data, &resp); err != nil {
		return nil, fmt.Errorf("decode mute: %w", err)
	}
	return &resp, nil
}

// PlayPause toggles play/pause.
func (c *Client) PlayPause() error {
	_, err := c.post("/current_play_state", PlayStateRequest{CurrentPlayState: PlayPauseToggle})
	return err
}

// Next skips to the next track.
func (c *Client) Next() error {
	_, err := c.post("/current_play_state", PlayStateRequest{CurrentPlayState: NextTrack})
	return err
}

// Prev goes to the previous track.
func (c *Client) Prev() error {
	_, err := c.post("/current_play_state", PlayStateRequest{CurrentPlayState: PrevTrack})
	return err
}

// ToggleMute toggles the mute state.
func (c *Client) ToggleMute() error {
	_, err := c.post("/remote_bar_order", RemoteBarRequest{BarControl: BarMute, Value: -1})
	return err
}

// TogglePower toggles the device power.
func (c *Client) TogglePower() error {
	_, err := c.post("/remote_bar_order", RemoteBarRequest{BarControl: BarPower, Value: -1})
	return err
}

// FetchAlbumArt downloads album artwork from the media library port.
func (c *Client) FetchAlbumArt(artID string) ([]byte, string, error) {
	url := c.mediaURL + "/v1/albumarts/" + artID
	resp, err := c.httpClient.Get(url)
	if err != nil {
		return nil, "", fmt.Errorf("fetch artwork: %w", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return nil, "", fmt.Errorf("artwork %s returned %d", artID, resp.StatusCode)
	}

	data, err := io.ReadAll(resp.Body)
	if err != nil {
		return nil, "", fmt.Errorf("read artwork: %w", err)
	}

	return data, resp.Header.Get("Content-Type"), nil
}

// isEOF checks if an error is an EOF, handling Go's HTTP/TLS wrapping.
func isEOF(err error) bool {
	if errors.Is(err, io.EOF) || errors.Is(err, io.ErrUnexpectedEOF) {
		return true
	}
	// Go's TLS layer sometimes wraps EOF in ways errors.Is cannot unwrap
	return strings.Contains(err.Error(), "EOF")
}
