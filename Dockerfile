# ==============================================================================
# Stage 1: Build Frontend SvelteKit (SPA Statis)
# ==============================================================================
FROM node:22-alpine AS frontend-builder

WORKDIR /frontend

COPY frontend/package.json frontend/package-lock.json ./
RUN npm ci --prefer-offline --no-audit

COPY frontend/ .
ENV VITE_USE_MOCK=false
RUN npm run build

# ==============================================================================
# Stage 2: Build Backend Go Fiber v3
# ==============================================================================
FROM golang:alpine AS backend-builder

ENV GOTOOLCHAIN=auto
WORKDIR /backend

COPY backend/go.mod backend/go.sum ./
RUN go mod download

COPY backend/ .
RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -ldflags="-s -w" -o /backend/server ./cmd/server && \
    CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -ldflags="-s -w" -o /backend/create-user ./cmd/create-user

# ==============================================================================
# Stage 3: Final Runtime Image (Backend + Frontend Static SPA dalam 1 Service)
# ==============================================================================
FROM alpine:3.20

RUN apk add --no-cache ca-certificates tzdata wget

WORKDIR /app

COPY --from=backend-builder /backend/server ./server
COPY --from=backend-builder /backend/create-user ./create-user
COPY --from=frontend-builder /frontend/build ./public

ENV APP_ENV=production
ENV STATIC_DIR=/app/public
ENV PORT=3000

EXPOSE 3000

CMD ["./server"]
