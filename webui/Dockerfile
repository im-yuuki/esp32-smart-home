# Multi-stage: node build -> static nginx.
FROM node:26-alpine AS build
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci
COPY . .
# build = vite build + vue-tsc type-check.
RUN npm run build

FROM nginx:alpine
# Minimal fallback config so the image works standalone (SPA only, no proxy).
# In the compose stack, the root nginx.conf is bind-mounted over this
# file and adds the /api + /ws reverse proxies to the server container.
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/dist /usr/share/nginx/html
EXPOSE 80
