# Multi-stage: node build -> static nginx.
FROM node:26-alpine AS build
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci
COPY . .
# build = vite build + vue-tsc type-check (sequential: the @nuxt/ui plugin
# generates auto-imports.d.ts/components.d.ts during the build step, which the
# type-check needs on a fresh tree).
RUN npm run build

FROM nginx:alpine
# Minimal fallback config so the image works standalone (SPA only, no proxy).
# In the compose stack, deploy/nginx/default.conf is bind-mounted over this
# file and adds the /api + /ws reverse proxies to the server container.
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/dist /usr/share/nginx/html
EXPOSE 80
