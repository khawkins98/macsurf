FROM golang:1.23 AS build
WORKDIR /src
COPY go.mod ./
COPY *.go ./
RUN CGO_ENABLED=0 go build -o /macsurf-proxy .

FROM scratch
COPY --from=build /etc/ssl/certs/ca-certificates.crt /etc/ssl/certs/
COPY --from=build /macsurf-proxy /macsurf-proxy
ENTRYPOINT ["/macsurf-proxy"]
