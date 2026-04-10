package ws

import (
	"encoding/json"
	"testing"
)

func TestParseCommandVolume(t *testing.T) {
	cmd, err := ParseCommand([]byte(`{"cmd":"volume","value":25}`))
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	if cmd.Cmd != CmdVolume {
		t.Errorf("expected cmd %q, got %q", CmdVolume, cmd.Cmd)
	}
	vol, err := VolumeFromCommand(cmd)
	if err != nil {
		t.Fatalf("volume: %v", err)
	}
	if vol != 25 {
		t.Errorf("expected 25, got %d", vol)
	}
}

func TestParseCommandPlayPause(t *testing.T) {
	cmd, err := ParseCommand([]byte(`{"cmd":"play_pause"}`))
	if err != nil {
		t.Fatalf("parse: %v", err)
	}
	if cmd.Cmd != CmdPlayPause {
		t.Errorf("expected cmd %q, got %q", CmdPlayPause, cmd.Cmd)
	}
}

func TestParseCommandAllValid(t *testing.T) {
	cmds := []string{CmdVolume, CmdPlayPause, CmdNext, CmdPrev, CmdMute, CmdPower}
	for _, name := range cmds {
		data, _ := json.Marshal(Command{Cmd: name})
		cmd, err := ParseCommand(data)
		if err != nil {
			t.Errorf("parse %q: %v", name, err)
			continue
		}
		if cmd.Cmd != name {
			t.Errorf("expected %q, got %q", name, cmd.Cmd)
		}
	}
}

func TestParseCommandUnknown(t *testing.T) {
	_, err := ParseCommand([]byte(`{"cmd":"explode"}`))
	if err == nil {
		t.Error("expected error for unknown command")
	}
}

func TestParseCommandEmpty(t *testing.T) {
	_, err := ParseCommand([]byte(`{}`))
	if err == nil {
		t.Error("expected error for missing cmd")
	}
}

func TestParseCommandInvalidJSON(t *testing.T) {
	_, err := ParseCommand([]byte(`not json`))
	if err == nil {
		t.Error("expected error for invalid JSON")
	}
}

func TestVolumeFromCommandMissingValue(t *testing.T) {
	cmd := &Command{Cmd: CmdVolume}
	_, err := VolumeFromCommand(cmd)
	if err == nil {
		t.Error("expected error for missing value")
	}
}

func TestVolumeFromCommandOutOfRange(t *testing.T) {
	cmd := &Command{Cmd: CmdVolume, Value: json.RawMessage(`150`)}
	_, err := VolumeFromCommand(cmd)
	if err == nil {
		t.Error("expected error for out-of-range volume")
	}
}

func TestVolumeFromCommandNegative(t *testing.T) {
	cmd := &Command{Cmd: CmdVolume, Value: json.RawMessage(`-5`)}
	_, err := VolumeFromCommand(cmd)
	if err == nil {
		t.Error("expected error for negative volume")
	}
}

func TestNewStateEvent(t *testing.T) {
	evt := NewStateEvent(42, true, true, "Song", "Artist", "Album", "tidal", "iPhone", 120000, 240000, true)
	if evt.Evt != EvtState {
		t.Errorf("expected evt %q, got %q", EvtState, evt.Evt)
	}
	if *evt.Volume != 42 {
		t.Errorf("expected volume 42, got %d", *evt.Volume)
	}
	if !*evt.Mute {
		t.Error("expected mute=true")
	}
	if evt.Title != "Song" {
		t.Errorf("expected title 'Song', got %q", evt.Title)
	}
	if *evt.Position != 120000 {
		t.Errorf("expected position 120000, got %d", *evt.Position)
	}
	if *evt.Duration != 240000 {
		t.Errorf("expected duration 240000, got %d", *evt.Duration)
	}
	if evt.TrackInfo != "iPhone" {
		t.Errorf("expected trackInfo 'iPhone', got %q", evt.TrackInfo)
	}
}

func TestNewVolumeEvent(t *testing.T) {
	evt := NewVolumeEvent(30)
	if evt.Evt != EvtVolume {
		t.Errorf("expected evt %q, got %q", EvtVolume, evt.Evt)
	}
	if *evt.Volume != 30 {
		t.Errorf("expected volume 30, got %d", *evt.Volume)
	}
}

func TestNewConnectedEvent(t *testing.T) {
	evt := NewConnectedEvent("RoseSalon")
	if evt.Device != "RoseSalon" {
		t.Errorf("expected device 'RoseSalon', got %q", evt.Device)
	}
}

func TestNewArtworkEvent(t *testing.T) {
	evt := NewArtworkEvent("/art/current.jpg")
	if evt.URL != "/art/current.jpg" {
		t.Errorf("expected URL '/art/current.jpg', got %q", evt.URL)
	}
}

func TestNewErrorEvent(t *testing.T) {
	evt := NewErrorEvent("something broke")
	if evt.Evt != EvtError {
		t.Errorf("expected evt %q, got %q", EvtError, evt.Evt)
	}
	if evt.Title != "something broke" {
		t.Errorf("expected 'something broke', got %q", evt.Title)
	}
}

func TestStateEventJSON(t *testing.T) {
	evt := NewStateEvent(25, false, true, "Song", "Artist", "Album", "radio", "", 0, 0, true)
	data, err := json.Marshal(evt)
	if err != nil {
		t.Fatalf("marshal: %v", err)
	}

	var decoded map[string]any
	json.Unmarshal(data, &decoded)

	if decoded["evt"] != "state" {
		t.Errorf("expected evt=state in JSON")
	}
	if decoded["volume"].(float64) != 25 {
		t.Errorf("expected volume=25 in JSON")
	}
}
