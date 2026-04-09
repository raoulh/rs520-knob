package rs520

import (
	"encoding/json"
	"fmt"
	"strconv"
)

// Playback control constants matching RS520 API.
const (
	PlayPauseToggle = 17
	NextTrack       = 18
	PrevTrack       = 19
	SeekTo          = 22
	RepeatToggle    = 24
	ShuffleToggle   = 25
)

// Remote bar control commands.
const (
	BarMute     = "remote_bar_order.mute"
	BarPower    = "remote_bar_order.power_onoff"
	BarReboot   = "remote_bar_order.reboot"
	BarScreenOn = "remote_bar_order.screen_onoff"
	BarHome     = "remote_bar_order.home"
)

// VolumeRequest sets the volume on the RS520.
type VolumeRequest struct {
	Volume int `json:"volume"`
}

// VolumeResponse is returned by POST /volume.
type VolumeResponse struct {
	VolumeValue int `json:"volumeValue"`
}

// PlayStateRequest controls playback (play/pause/next/prev/seek).
type PlayStateRequest struct {
	CurrentPlayState  int `json:"currentPlayState"`
	CurrentPlaySeekTo int `json:"currentPlaySeekto,omitempty"`
}

// RemoteBarRequest sends virtual remote control commands.
type RemoteBarRequest struct {
	BarControl string `json:"barControl"`
	Value      int    `json:"value"`
}

// DeviceConnectedRequest registers a controller with the RS520.
type DeviceConnectedRequest struct {
	ConnectIP string `json:"connectIP"`
}

// MuteResponse is returned by POST /mute.state.get.
type MuteResponse struct {
	Mute bool `json:"mute"`
}

// DeviceNameResponse wraps /device_name response.
type DeviceNameResponse struct {
	Data json.RawMessage `json:"data"`
}

// CurrentStateResponse wraps /get_current_state response.
type CurrentStateResponse struct {
	Data CurrentStateData `json:"data"`
}

// FlexInt handles JSON values that may arrive as either a number or a string.
type FlexInt int

func (fi *FlexInt) UnmarshalJSON(b []byte) error {
	// Try number first.
	var n int
	if err := json.Unmarshal(b, &n); err == nil {
		*fi = FlexInt(n)
		return nil
	}
	// Fall back to string.
	var s string
	if err := json.Unmarshal(b, &s); err != nil {
		return err
	}
	if s == "" {
		*fi = 0
		return nil
	}
	parsed, err := strconv.Atoi(s)
	if err != nil {
		return fmt.Errorf("FlexInt: cannot parse %q: %w", s, err)
	}
	*fi = FlexInt(parsed)
	return nil
}

// CurrentStateData holds playback state fields.
type CurrentStateData struct {
	TitleName         string   `json:"titleName"`
	SubAppCurrentData string   `json:"subAppCurrentData"`
	TempArr           []string `json:"tempArr"`
	AlbumArtID        string   `json:"albumArtId"`
	ArtistName        string   `json:"artistName"`
	AlbumName         string   `json:"albumName"`
	Duration          FlexInt  `json:"duration"`
	CurrentPosition   FlexInt  `json:"currentPosition"`
	SourceName        string   `json:"sourceName"`
}

// ControlInfoResponse wraps GET /get_control_info response.
type ControlInfoResponse struct {
	Volume       int    `json:"volume"`
	PlayState    int    `json:"playState"`
	Source       string `json:"source"`
	Mute         bool   `json:"mute"`
	PlayingState string `json:"playingState"`
}

// NotificationMessage is sent by the RS520 to :9284.
type NotificationMessage struct {
	MessageType string          `json:"messageType"`
	Volume      int             `json:"volume,omitempty"`
	Position    int             `json:"position,omitempty"`
	DataObj     json.RawMessage `json:"dataObj,omitempty"`
}
