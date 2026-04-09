package state

import (
	"sync"
	"testing"
)

func TestSnapshotReturnsCurrentState(t *testing.T) {
	c := NewCache()
	c.Update(State{Volume: 42, Mute: true, Playing: true, Title: "Song", Artist: "Band"})

	s := c.Snapshot()
	if s.Volume != 42 {
		t.Errorf("expected volume 42, got %d", s.Volume)
	}
	if !s.Mute {
		t.Error("expected mute=true")
	}
	if s.Title != "Song" {
		t.Errorf("expected title 'Song', got %q", s.Title)
	}
}

func TestUpdateReturnsChanges(t *testing.T) {
	c := NewCache()
	c.Update(State{Volume: 20, Title: "A", Artist: "B"})

	changes := c.Update(State{Volume: 30, Title: "A", Artist: "C"})

	hasVolume := false
	hasArtist := false
	for _, ch := range changes {
		switch ch.Field {
		case "volume":
			hasVolume = true
			if ch.OldValue != 20 || ch.NewValue != 30 {
				t.Errorf("volume change wrong: %v → %v", ch.OldValue, ch.NewValue)
			}
		case "artist":
			hasArtist = true
		case "title":
			t.Error("title should not have changed")
		}
	}
	if !hasVolume {
		t.Error("missing volume change")
	}
	if !hasArtist {
		t.Error("missing artist change")
	}
}

func TestUpdateNoChanges(t *testing.T) {
	c := NewCache()
	c.Update(State{Volume: 20, Title: "A"})

	changes := c.Update(State{Volume: 20, Title: "A"})
	if len(changes) != 0 {
		t.Errorf("expected no changes, got %d", len(changes))
	}
}

func TestSetVolume(t *testing.T) {
	c := NewCache()
	c.Update(State{Volume: 10})

	changes := c.SetVolume(25)
	if len(changes) != 1 || changes[0].Field != "volume" {
		t.Errorf("expected volume change, got %v", changes)
	}

	s := c.Snapshot()
	if s.Volume != 25 {
		t.Errorf("expected volume 25, got %d", s.Volume)
	}
}

func TestSetVolumeSameValue(t *testing.T) {
	c := NewCache()
	c.Update(State{Volume: 10})

	changes := c.SetVolume(10)
	if len(changes) != 0 {
		t.Error("expected no changes for same volume")
	}
}

func TestSetMute(t *testing.T) {
	c := NewCache()
	changes := c.SetMute(true)
	if len(changes) != 1 || changes[0].Field != "mute" {
		t.Errorf("expected mute change, got %v", changes)
	}
	if !c.Snapshot().Mute {
		t.Error("expected mute=true")
	}
}

func TestConcurrentAccess(t *testing.T) {
	c := NewCache()
	var wg sync.WaitGroup

	for i := 0; i < 100; i++ {
		wg.Add(2)
		go func(v int) {
			defer wg.Done()
			c.SetVolume(v)
		}(i)
		go func() {
			defer wg.Done()
			c.Snapshot()
		}()
	}

	wg.Wait()
}

func TestSetPlaying(t *testing.T) {
	c := NewCache()
	changes := c.SetPlaying(true)
	if len(changes) != 1 || changes[0].Field != "playing" {
		t.Errorf("expected playing change, got %v", changes)
	}

	changes = c.SetPlaying(true)
	if len(changes) != 0 {
		t.Error("expected no changes for same playing state")
	}
}

func TestUpdateAllFieldsChanged(t *testing.T) {
	c := NewCache()
	changes := c.Update(State{
		Volume:     50,
		Mute:       true,
		Playing:    true,
		Title:      "T",
		Artist:     "A",
		Album:      "Al",
		AlbumArtID: "id",
		PowerOn:    true,
		Source:     "tidal",
	})

	// Default is PowerOn=true, so that won't change. All others should.
	expected := map[string]bool{
		"volume": true, "mute": true, "playing": true,
		"title": true, "artist": true, "album": true,
		"albumArtId": true, "source": true,
	}
	for _, ch := range changes {
		delete(expected, ch.Field)
	}
	if len(expected) != 0 {
		t.Errorf("missing changes for: %v", expected)
	}
}
