package artwork

import (
	"bytes"
	"crypto/md5"
	"encoding/binary"
	"fmt"
	"image"
	"image/color"
	"image/jpeg"
	_ "image/png"
	"log"
	"net/http"
	"sync"

	_ "golang.org/x/image/bmp"
	"golang.org/x/image/draw"
	_ "golang.org/x/image/webp"
)

// Proxy fetches, resizes, and caches album artwork.
type Proxy struct {
	rs520MediaURL string
	httpClient    *http.Client
	cache         *lruCache
}

// NewProxy creates an artwork proxy.
func NewProxy(rs520Host string, httpClient *http.Client) *Proxy {
	if httpClient == nil {
		httpClient = &http.Client{}
	}
	return &Proxy{
		rs520MediaURL: fmt.Sprintf("http://%s:8000", rs520Host),
		httpClient:    httpClient,
		cache:         newLRUCache(5),
	}
}

// NewProxyWithURL creates a proxy with an explicit media URL (for testing).
func NewProxyWithURL(mediaURL string, httpClient *http.Client) *Proxy {
	return &Proxy{
		rs520MediaURL: mediaURL,
		httpClient:    httpClient,
		cache:         newLRUCache(5),
	}
}

// ServeHTTP handles GET /art/current?width=360&height=360&format=rgb565|jpeg
func (p *Proxy) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	artID := r.URL.Query().Get("id")
	artURL := r.URL.Query().Get("url")
	if artID == "" && artURL == "" {
		http.Error(w, "missing id or url parameter", http.StatusBadRequest)
		return
	}

	format := r.URL.Query().Get("format")
	if format == "" {
		format = "jpeg"
	}
	if format != "jpeg" && format != "rgb565" {
		http.Error(w, "format must be jpeg or rgb565", http.StatusBadRequest)
		return
	}

	width := 360
	height := 360

	// Use artID or hash of URL as cache key
	sourceKey := artID
	if sourceKey == "" {
		sourceKey = fmt.Sprintf("url-%x", md5.Sum([]byte(artURL)))
	}
	cacheKey := fmt.Sprintf("%s-%s-%dx%d", sourceKey, format, width, height)

	// ETag support
	etag := fmt.Sprintf(`"%x"`, md5.Sum([]byte(cacheKey)))
	if r.Header.Get("If-None-Match") == etag {
		w.WriteHeader(http.StatusNotModified)
		return
	}

	// Check cache
	if data, ok := p.cache.Get(cacheKey); ok {
		writeResponse(w, data, format, etag)
		return
	}

	// Fetch source image
	var fetchURL string
	if artURL != "" {
		fetchURL = artURL
	} else {
		fetchURL = p.rs520MediaURL + "/v1/albumarts/" + artID
	}
	resp, err := p.httpClient.Get(fetchURL)
	if err != nil {
		log.Printf("[artwork] fetch error: %v", err)
		http.Error(w, "fetch error", http.StatusBadGateway)
		return
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		http.Error(w, "artwork not found", resp.StatusCode)
		return
	}

	// Decode
	src, fmtName, err := image.Decode(resp.Body)
	if err != nil {
		log.Printf("[artwork] decode error: %v (content-type: %s, url: %s)", err, resp.Header.Get("Content-Type"), fetchURL)
		http.Error(w, "decode error", http.StatusInternalServerError)
		return
	}
	_ = fmtName

	// Center-crop to square, then resize
	srcBounds := src.Bounds()
	srcW := srcBounds.Dx()
	srcH := srcBounds.Dy()
	cropRect := srcBounds
	if srcW > srcH {
		offset := (srcW - srcH) / 2
		cropRect = image.Rect(srcBounds.Min.X+offset, srcBounds.Min.Y, srcBounds.Min.X+offset+srcH, srcBounds.Max.Y)
	} else if srcH > srcW {
		offset := (srcH - srcW) / 2
		cropRect = image.Rect(srcBounds.Min.X, srcBounds.Min.Y+offset, srcBounds.Max.X, srcBounds.Min.Y+offset+srcW)
	}

	dst := image.NewRGBA(image.Rect(0, 0, width, height))
	draw.BiLinear.Scale(dst, dst.Bounds(), src, cropRect, draw.Over, nil)

	// Encode to requested format
	var output []byte
	switch format {
	case "jpeg":
		var buf bytes.Buffer
		if err := jpeg.Encode(&buf, dst, &jpeg.Options{Quality: 85}); err != nil {
			http.Error(w, "encode error", http.StatusInternalServerError)
			return
		}
		output = buf.Bytes()
	case "rgb565":
		output = rgbaToRGB565BigEndian(dst)
	}

	p.cache.Put(cacheKey, output)
	writeResponse(w, output, format, etag)
}

func writeResponse(w http.ResponseWriter, data []byte, format, etag string) {
	w.Header().Set("ETag", etag)
	w.Header().Set("Cache-Control", "max-age=300")
	switch format {
	case "jpeg":
		w.Header().Set("Content-Type", "image/jpeg")
	case "rgb565":
		w.Header().Set("Content-Type", "application/octet-stream")
	}
	w.Header().Set("Content-Length", fmt.Sprintf("%d", len(data)))
	w.Write(data)
}

// rgbaToRGB565BigEndian converts RGBA image to RGB565 big-endian bytes.
// R: bits 15-11, G: bits 10-5, B: bits 4-0, big-endian byte order.
func rgbaToRGB565BigEndian(img *image.RGBA) []byte {
	bounds := img.Bounds()
	w := bounds.Dx()
	h := bounds.Dy()
	out := make([]byte, w*h*2)

	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			c := img.RGBAAt(x+bounds.Min.X, y+bounds.Min.Y)
			pixel := rgb565(c)
			offset := (y*w + x) * 2
			binary.BigEndian.PutUint16(out[offset:], pixel)
		}
	}
	return out
}

func rgb565(c color.RGBA) uint16 {
	r := uint16(c.R) >> 3
	g := uint16(c.G) >> 2
	b := uint16(c.B) >> 3
	return (r << 11) | (g << 5) | b
}

// Simple LRU cache.
type lruEntry struct {
	key  string
	data []byte
}

type lruCache struct {
	mu      sync.Mutex
	maxSize int
	entries []lruEntry
}

func newLRUCache(maxSize int) *lruCache {
	return &lruCache{maxSize: maxSize}
}

func (c *lruCache) Get(key string) ([]byte, bool) {
	c.mu.Lock()
	defer c.mu.Unlock()
	for i, e := range c.entries {
		if e.key == key {
			// Move to front
			c.entries = append(c.entries[:i], c.entries[i+1:]...)
			c.entries = append([]lruEntry{e}, c.entries...)
			return e.data, true
		}
	}
	return nil, false
}

func (c *lruCache) Put(key string, data []byte) {
	c.mu.Lock()
	defer c.mu.Unlock()

	// Remove existing
	for i, e := range c.entries {
		if e.key == key {
			c.entries = append(c.entries[:i], c.entries[i+1:]...)
			break
		}
	}

	c.entries = append([]lruEntry{{key: key, data: data}}, c.entries...)

	// Evict oldest if over capacity
	if len(c.entries) > c.maxSize {
		c.entries = c.entries[:c.maxSize]
	}
}
