package ws

import (
	"encoding/json"
	"fmt"
)

// Command is a message from the knob to the bridge.
type Command struct {
	Cmd   string          `json:"cmd"`
	Value json.RawMessage `json:"value,omitempty"`
}

// Event is a message from the bridge to the knob.
type Event struct {
	Evt     string `json:"evt"`
	Volume  *int   `json:"volume,omitempty"`
	Mute    *bool  `json:"mute,omitempty"`
	Playing *bool  `json:"playing,omitempty"`
	Title   string `json:"title,omitempty"`
	Artist  string `json:"artist,omitempty"`
	Album   string `json:"album,omitempty"`
	Source  string `json:"source,omitempty"`
	PowerOn *bool  `json:"powerOn,omitempty"`
	Device  string `json:"device,omitempty"`
	URL     string `json:"url,omitempty"`
}

// Valid command names.
const (
	CmdVolume    = "volume"
	CmdPlayPause = "play_pause"
	CmdNext      = "next"
	CmdPrev      = "prev"
	CmdMute      = "mute"
	CmdPower     = "power"
)

// Event type names.
const (
	EvtState     = "state"
	EvtVolume    = "volume"
	EvtConnected = "connected"
	EvtArtwork   = "artwork"
	EvtError     = "error"
)

// ParseCommand parses and validates a JSON command.
func ParseCommand(data []byte) (*Command, error) {
	var cmd Command
	if err := json.Unmarshal(data, &cmd); err != nil {
		return nil, fmt.Errorf("invalid JSON: %w", err)
	}
	switch cmd.Cmd {
	case CmdVolume, CmdPlayPause, CmdNext, CmdPrev, CmdMute, CmdPower:
		return &cmd, nil
	case "":
		return nil, fmt.Errorf("missing cmd field")
	default:
		return nil, fmt.Errorf("unknown command: %q", cmd.Cmd)
	}
}

// VolumeFromCommand extracts the integer volume value from a volume command.
func VolumeFromCommand(cmd *Command) (int, error) {
	if cmd.Value == nil {
		return 0, fmt.Errorf("volume command missing value")
	}
	var vol int
	if err := json.Unmarshal(cmd.Value, &vol); err != nil {
		return 0, fmt.Errorf("invalid volume value: %w", err)
	}
	if vol < 0 || vol > 100 {
		return 0, fmt.Errorf("volume out of range: %d", vol)
	}
	return vol, nil
}

// NewStateEvent creates a full state event for sending to the knob.
func NewStateEvent(volume int, mute, playing bool, title, artist, album, source string, powerOn bool) *Event {
	return &Event{
		Evt:     EvtState,
		Volume:  &volume,
		Mute:    &mute,
		Playing: &playing,
		Title:   title,
		Artist:  artist,
		Album:   album,
		Source:   source,
		PowerOn: &powerOn,
	}
}

// NewVolumeEvent creates a volume change event.
func NewVolumeEvent(volume int) *Event {
	return &Event{
		Evt:    EvtVolume,
		Volume: &volume,
	}
}

// NewConnectedEvent creates a connected event.
func NewConnectedEvent(device string) *Event {
	return &Event{
		Evt:    EvtConnected,
		Device: device,
	}
}

// NewArtworkEvent creates an artwork URL event.
func NewArtworkEvent(url string) *Event {
	return &Event{
		Evt: EvtArtwork,
		URL: url,
	}
}

// NewErrorEvent creates an error event.
func NewErrorEvent(msg string) *Event {
	return &Event{
		Evt:   EvtError,
		Title: msg,
	}
}
