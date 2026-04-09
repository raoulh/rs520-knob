package artwork

import (
	"bytes"
	"image"
	"image/color"
	"image/jpeg"
	"net/http"
	"net/http/httptest"
	"testing"
)

func createTestJPEG(t *testing.T, w, h int) []byte {
	t.Helper()
	img := image.NewRGBA(image.Rect(0, 0, w, h))
	// Fill with known color
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			img.Set(x, y, color.RGBA{R: 255, G: 0, B: 0, A: 255})
		}
	}
	var buf bytes.Buffer
	if err := jpeg.Encode(&buf, img, &jpeg.Options{Quality: 90}); err != nil {
		t.Fatalf("encode test jpeg: %v", err)
	}
	return buf.Bytes()
}

func TestProxyJPEG(t *testing.T) {
	testJPEG := createTestJPEG(t, 800, 800)

	// Mock RS520 media server
	mediaSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path != "/v1/albumarts/test123" {
			t.Errorf("unexpected path: %s", r.URL.Path)
			http.NotFound(w, r)
			return
		}
		w.Header().Set("Content-Type", "image/jpeg")
		w.Write(testJPEG)
	}))
	defer mediaSrv.Close()

	proxy := NewProxyWithURL(mediaSrv.URL, mediaSrv.Client())

	req := httptest.NewRequest(http.MethodGet, "/art/current?id=test123&format=jpeg", nil)
	w := httptest.NewRecorder()
	proxy.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	if w.Header().Get("Content-Type") != "image/jpeg" {
		t.Errorf("expected image/jpeg, got %s", w.Header().Get("Content-Type"))
	}
	if len(w.Body.Bytes()) == 0 {
		t.Error("expected non-empty body")
	}

	// Verify it's a valid JPEG
	_, err := jpeg.Decode(bytes.NewReader(w.Body.Bytes()))
	if err != nil {
		t.Errorf("output is not valid JPEG: %v", err)
	}
}

func TestProxyRGB565(t *testing.T) {
	testJPEG := createTestJPEG(t, 100, 100)

	mediaSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "image/jpeg")
		w.Write(testJPEG)
	}))
	defer mediaSrv.Close()

	proxy := NewProxyWithURL(mediaSrv.URL, mediaSrv.Client())

	req := httptest.NewRequest(http.MethodGet, "/art/current?id=abc&format=rgb565", nil)
	w := httptest.NewRecorder()
	proxy.ServeHTTP(w, req)

	if w.Code != http.StatusOK {
		t.Fatalf("expected 200, got %d", w.Code)
	}
	// 360 * 360 * 2 = 259200 bytes
	expectedSize := 360 * 360 * 2
	if len(w.Body.Bytes()) != expectedSize {
		t.Errorf("expected %d bytes, got %d", expectedSize, len(w.Body.Bytes()))
	}
	if w.Header().Get("Content-Type") != "application/octet-stream" {
		t.Errorf("expected application/octet-stream, got %s", w.Header().Get("Content-Type"))
	}
}

func TestProxyCacheHit(t *testing.T) {
	callCount := 0
	testJPEG := createTestJPEG(t, 50, 50)

	mediaSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		callCount++
		w.Header().Set("Content-Type", "image/jpeg")
		w.Write(testJPEG)
	}))
	defer mediaSrv.Close()

	proxy := NewProxyWithURL(mediaSrv.URL, mediaSrv.Client())

	// First request — cache miss
	req := httptest.NewRequest(http.MethodGet, "/art/current?id=x&format=jpeg", nil)
	w := httptest.NewRecorder()
	proxy.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("first request: expected 200, got %d", w.Code)
	}

	// Second request — cache hit
	req = httptest.NewRequest(http.MethodGet, "/art/current?id=x&format=jpeg", nil)
	w = httptest.NewRecorder()
	proxy.ServeHTTP(w, req)
	if w.Code != http.StatusOK {
		t.Fatalf("second request: expected 200, got %d", w.Code)
	}

	if callCount != 1 {
		t.Errorf("expected 1 upstream call, got %d", callCount)
	}
}

func TestProxyETag304(t *testing.T) {
	testJPEG := createTestJPEG(t, 50, 50)

	mediaSrv := httptest.NewServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Content-Type", "image/jpeg")
		w.Write(testJPEG)
	}))
	defer mediaSrv.Close()

	proxy := NewProxyWithURL(mediaSrv.URL, mediaSrv.Client())

	// First request to populate cache + get ETag
	req := httptest.NewRequest(http.MethodGet, "/art/current?id=e&format=jpeg", nil)
	w := httptest.NewRecorder()
	proxy.ServeHTTP(w, req)
	etag := w.Header().Get("ETag")
	if etag == "" {
		t.Fatal("expected ETag header")
	}

	// Second request with If-None-Match
	req = httptest.NewRequest(http.MethodGet, "/art/current?id=e&format=jpeg", nil)
	req.Header.Set("If-None-Match", etag)
	w = httptest.NewRecorder()
	proxy.ServeHTTP(w, req)
	if w.Code != http.StatusNotModified {
		t.Errorf("expected 304, got %d", w.Code)
	}
}

func TestProxyMissingID(t *testing.T) {
	proxy := NewProxyWithURL("http://unused", nil)
	req := httptest.NewRequest(http.MethodGet, "/art/current?format=jpeg", nil)
	w := httptest.NewRecorder()
	proxy.ServeHTTP(w, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", w.Code)
	}
}

func TestProxyInvalidFormat(t *testing.T) {
	proxy := NewProxyWithURL("http://unused", nil)
	req := httptest.NewRequest(http.MethodGet, "/art/current?id=x&format=png", nil)
	w := httptest.NewRecorder()
	proxy.ServeHTTP(w, req)
	if w.Code != http.StatusBadRequest {
		t.Errorf("expected 400, got %d", w.Code)
	}
}

func TestRGB565Conversion(t *testing.T) {
	img := image.NewRGBA(image.Rect(0, 0, 1, 1))
	// Pure red: R=255, G=0, B=0
	// RGB565: R=31<<11 = 0xF800, big-endian: [0xF8, 0x00]
	img.Set(0, 0, color.RGBA{R: 255, G: 0, B: 0, A: 255})

	result := rgbaToRGB565BigEndian(img)
	if len(result) != 2 {
		t.Fatalf("expected 2 bytes, got %d", len(result))
	}
	if result[0] != 0xF8 || result[1] != 0x00 {
		t.Errorf("expected [0xF8, 0x00], got [0x%02X, 0x%02X]", result[0], result[1])
	}
}

func TestRGB565Green(t *testing.T) {
	img := image.NewRGBA(image.Rect(0, 0, 1, 1))
	// Pure green: R=0, G=255, B=0
	// RGB565: G=63<<5 = 0x07E0, big-endian: [0x07, 0xE0]
	img.Set(0, 0, color.RGBA{R: 0, G: 255, B: 0, A: 255})

	result := rgbaToRGB565BigEndian(img)
	if result[0] != 0x07 || result[1] != 0xE0 {
		t.Errorf("expected [0x07, 0xE0], got [0x%02X, 0x%02X]", result[0], result[1])
	}
}

func TestRGB565Blue(t *testing.T) {
	img := image.NewRGBA(image.Rect(0, 0, 1, 1))
	// Pure blue: R=0, G=0, B=255
	// RGB565: B=31 = 0x001F, big-endian: [0x00, 0x1F]
	img.Set(0, 0, color.RGBA{R: 0, G: 0, B: 255, A: 255})

	result := rgbaToRGB565BigEndian(img)
	if result[0] != 0x00 || result[1] != 0x1F {
		t.Errorf("expected [0x00, 0x1F], got [0x%02X, 0x%02X]", result[0], result[1])
	}
}

func TestLRUCacheEviction(t *testing.T) {
	c := newLRUCache(2)

	c.Put("a", []byte("A"))
	c.Put("b", []byte("B"))
	c.Put("c", []byte("C")) // should evict "a"

	if _, ok := c.Get("a"); ok {
		t.Error("expected 'a' to be evicted")
	}
	if _, ok := c.Get("b"); !ok {
		t.Error("expected 'b' to be in cache")
	}
	if _, ok := c.Get("c"); !ok {
		t.Error("expected 'c' to be in cache")
	}
}

func TestLRUCacheAccessRefreshes(t *testing.T) {
	c := newLRUCache(2)

	c.Put("a", []byte("A"))
	c.Put("b", []byte("B"))
	c.Get("a") // refresh "a"
	c.Put("c", []byte("C")) // should evict "b" (least recently used)

	if _, ok := c.Get("a"); !ok {
		t.Error("expected 'a' to be in cache after refresh")
	}
	if _, ok := c.Get("b"); ok {
		t.Error("expected 'b' to be evicted")
	}
}
