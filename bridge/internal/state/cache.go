package state

import (
	"sync"
)

// Change represents a single field change in the state.
type Change struct {
	Field    string
	OldValue any
	NewValue any
}

// State holds the cached RS520 device state.
type State struct {
	Volume     int    `json:"volume"`
	Mute       bool   `json:"mute"`
	Playing    bool   `json:"playing"`
	Title      string `json:"title"`
	Artist     string `json:"artist"`
	Album      string `json:"album"`
	AlbumArtID string `json:"albumArtId"`
	PowerOn    bool   `json:"powerOn"`
	Source     string `json:"source"`
}

// Cache provides thread-safe access to the device state.
type Cache struct {
	mu    sync.RWMutex
	state State
}

// NewCache creates a new state cache.
func NewCache() *Cache {
	return &Cache{
		state: State{PowerOn: true},
	}
}

// Snapshot returns a copy of the current state.
func (c *Cache) Snapshot() State {
	c.mu.RLock()
	defer c.mu.RUnlock()
	return c.state
}

// Update replaces the state and returns a list of changes.
func (c *Cache) Update(new State) []Change {
	c.mu.Lock()
	defer c.mu.Unlock()

	var changes []Change
	old := c.state

	if old.Volume != new.Volume {
		changes = append(changes, Change{"volume", old.Volume, new.Volume})
	}
	if old.Mute != new.Mute {
		changes = append(changes, Change{"mute", old.Mute, new.Mute})
	}
	if old.Playing != new.Playing {
		changes = append(changes, Change{"playing", old.Playing, new.Playing})
	}
	if old.Title != new.Title {
		changes = append(changes, Change{"title", old.Title, new.Title})
	}
	if old.Artist != new.Artist {
		changes = append(changes, Change{"artist", old.Artist, new.Artist})
	}
	if old.Album != new.Album {
		changes = append(changes, Change{"album", old.Album, new.Album})
	}
	if old.AlbumArtID != new.AlbumArtID {
		changes = append(changes, Change{"albumArtId", old.AlbumArtID, new.AlbumArtID})
	}
	if old.PowerOn != new.PowerOn {
		changes = append(changes, Change{"powerOn", old.PowerOn, new.PowerOn})
	}
	if old.Source != new.Source {
		changes = append(changes, Change{"source", old.Source, new.Source})
	}

	c.state = new
	return changes
}

// SetVolume updates only the volume field.
func (c *Cache) SetVolume(vol int) []Change {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.state.Volume == vol {
		return nil
	}
	old := c.state.Volume
	c.state.Volume = vol
	return []Change{{"volume", old, vol}}
}

// SetMute updates only the mute field.
func (c *Cache) SetMute(mute bool) []Change {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.state.Mute == mute {
		return nil
	}
	old := c.state.Mute
	c.state.Mute = mute
	return []Change{{"mute", old, mute}}
}

// SetPlaying updates only the playing field.
func (c *Cache) SetPlaying(playing bool) []Change {
	c.mu.Lock()
	defer c.mu.Unlock()
	if c.state.Playing == playing {
		return nil
	}
	old := c.state.Playing
	c.state.Playing = playing
	return []Change{{"playing", old, playing}}
}
