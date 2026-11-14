# Multi-stage build untuk Go Fiber backend
FROM golang:1.26.5-alpine AS builder

WORKDIR /build

# Copy go.mod dan go.sum untuk cache dependencies
COPY go.mod go.sum ./

# Download dependencies
RUN go mod download

# Copy seluruh source code
COPY . .

# Build aplikasi (CGO_ENABLED=0 untuk static binary yang portable)
RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -ldflags="-s -w" -o /build/server ./cmd/server && \
    CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -ldflags="-s -w" -o /build/create-user ./cmd/create-user

# Runtime image yang minimal
FROM alpine:3.20

RUN apk add --no-cache ca-certificates tzdata

WORKDIR /app

# Copy binary dari builder
COPY --from=builder /build/server .
COPY --from=builder /build/create-user .

# Ekspos port sesuai default APP_PORT
EXPOSE 3000

# Health check
HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD wget --no-verbose --tries=1 --spider http://localhost:3000/api/health || exit 1

# Jalankan aplikasi
CMD ["./server"]
