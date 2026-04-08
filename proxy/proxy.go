package main

import (
	"io"
	"log"
	"net"
	"net/http"
	"time"
)

type Proxy struct {
	Auth *Credentials
}

func (p *Proxy) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if p.Auth != nil && !p.Auth.Check(r) {
		w.Header().Set("Proxy-Authenticate", `Basic realm="macsurf-proxy"`)
		http.Error(w, "Proxy authentication required", http.StatusProxyAuthRequired)
		return
	}

	if r.Method == http.MethodConnect {
		p.handleConnect(w, r)
	} else {
		p.handleHTTP(w, r)
	}
}

func (p *Proxy) handleHTTP(w http.ResponseWriter, r *http.Request) {
	if r.URL.Host == "" {
		http.Error(w, "missing host in request", http.StatusBadRequest)
		return
	}

	outReq, err := http.NewRequestWithContext(r.Context(), r.Method, r.URL.String(), r.Body)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	copyHeaders(outReq.Header, r.Header)
	outReq.Header.Del("Proxy-Authorization")
	outReq.Header.Del("Proxy-Connection")

	resp, err := http.DefaultTransport.RoundTrip(outReq)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}
	defer resp.Body.Close()

	copyHeaders(w.Header(), resp.Header)
	w.WriteHeader(resp.StatusCode)
	io.Copy(w, resp.Body)
}

func (p *Proxy) handleConnect(w http.ResponseWriter, r *http.Request) {
	dest, err := net.DialTimeout("tcp", r.Host, 10*time.Second)
	if err != nil {
		http.Error(w, err.Error(), http.StatusBadGateway)
		return
	}

	hj, ok := w.(http.Hijacker)
	if !ok {
		dest.Close()
		http.Error(w, "hijacking not supported", http.StatusInternalServerError)
		return
	}

	w.WriteHeader(http.StatusOK)

	client, _, err := hj.Hijack()
	if err != nil {
		dest.Close()
		log.Printf("hijack error: %v", err)
		return
	}

	go transfer(dest, client)
	go transfer(client, dest)
}

const idleTimeout = 10 * time.Minute

func transfer(dst, src net.Conn) {
	defer dst.Close()
	defer src.Close()

	buf := make([]byte, 32*1024)
	for {
		src.SetDeadline(time.Now().Add(idleTimeout))
		n, readErr := src.Read(buf)
		if n > 0 {
			dst.SetDeadline(time.Now().Add(idleTimeout))
			if _, writeErr := dst.Write(buf[:n]); writeErr != nil {
				return
			}
		}
		if readErr != nil {
			return
		}
	}
}

func copyHeaders(dst, src http.Header) {
	for k, vv := range src {
		for _, v := range vv {
			dst.Add(k, v)
		}
	}
}
