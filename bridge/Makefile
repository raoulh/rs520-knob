.PHONY: build test lint clean docker-build docker-up integration-test

BINARY := bridge
RS520_HOST ?= 192.168.30.135

build:
	go build -o $(BINARY) ./cmd/bridge

test:
	go test ./... -v -count=1

lint:
	go vet ./...
	@which golangci-lint > /dev/null 2>&1 && golangci-lint run || echo "golangci-lint not installed, skipping"

clean:
	rm -f $(BINARY)

docker-build:
	docker build -t rs520-bridge .

docker-up:
	docker compose up --build

integration-test:
	RS520_HOST=$(RS520_HOST) go test -tags=integration -v ./...

curl-test:
	bash scripts/curl_integration_test.sh $(RS520_HOST)
