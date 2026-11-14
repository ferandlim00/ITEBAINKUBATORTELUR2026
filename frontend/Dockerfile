# Multi-stage build untuk SvelteKit frontend
FROM node:22-alpine AS builder

WORKDIR /build

# Copy package files
COPY package.json package-lock.json ./

# Install dependencies
RUN npm ci --prefer-offline --no-audit

# Copy source code
COPY . .

# Set environment untuk production build (tidak mock)
ENV VITE_USE_MOCK=false

# Build static site
RUN npm run build

# Serve static files dengan nginx
FROM nginx:alpine

# Copy custom nginx config (optional, bisa skip jika default sudah cukup)
COPY nginx.conf /etc/nginx/conf.d/default.conf

# Copy build output ke nginx
COPY --from=builder /build/build /usr/share/nginx/html

EXPOSE 80

HEALTHCHECK --interval=30s --timeout=5s --start-period=10s --retries=3 \
    CMD wget --no-verbose --tries=1 --spider http://localhost/ || exit 1

CMD ["nginx", "-g", "daemon off;"]
