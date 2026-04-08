# MacSurf Proxy

A TLS-stripping HTTP proxy for MacSurf. Receives plain HTTP from a Mac OS 9 client, fetches via HTTPS, returns plain HTTP. Single binary, zero dependencies.

## Build

```
cd proxy
go build -o macsurf-proxy .
```

## Usage

```
# Default — listens on :8765
./macsurf-proxy

# Custom port
./macsurf-proxy --port 9000

# With basic auth
./macsurf-proxy --auth user:password
```

## Test

```
# Plain HTTP forwarding
curl -x http://localhost:8765 http://example.com

# HTTPS via CONNECT tunnel
curl -x http://localhost:8765 https://example.com

# With auth
curl -x http://user:password@localhost:8765 https://example.com
```

## Deploy

### Local network

Run on any machine the Mac can reach. Point the Mac's HTTP proxy setting to `<machine-ip>:8765`.

### VPS

```
scp macsurf-proxy you@vps:/usr/local/bin/
ssh you@vps 'macsurf-proxy --port 8765 --auth user:password &'
```

### Docker

```dockerfile
FROM golang:1.22-alpine AS build
WORKDIR /src
COPY . .
RUN go build -o /macsurf-proxy .

FROM alpine:3.19
COPY --from=build /macsurf-proxy /usr/local/bin/
EXPOSE 8765
ENTRYPOINT ["macsurf-proxy"]
```

```
docker build -t macsurf-proxy .
docker run -d -p 8765:8765 macsurf-proxy --auth user:password
```

## Mac OS 9 Client Configuration

In the MacSurf browser preferences (or any HTTP proxy-aware browser like Classilla):

- **HTTP Proxy:** `<proxy-host>`
- **Port:** `8765`

All HTTPS sites will be fetched by the proxy and delivered as plain HTTP to the Mac.
