FROM golang:1.22-alpine AS builder

RUN apk add --no-cache git ca-certificates

WORKDIR /src
COPY go.mod go.sum ./
RUN go mod download

COPY . .
RUN CGO_ENABLED=0 GOOS=linux go build -ldflags="-s -w" -o /bridge ./cmd/bridge

FROM alpine:3.20

RUN apk add --no-cache ca-certificates
COPY --from=builder /bridge /bridge

EXPOSE 8080 9284

ENTRYPOINT ["/bridge"]
