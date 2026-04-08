package main

import (
	"encoding/base64"
	"fmt"
	"net/http"
	"strings"
)

type Credentials struct {
	User string
	Pass string
}

func ParseCredentials(s string) (*Credentials, error) {
	parts := strings.SplitN(s, ":", 2)
	if len(parts) != 2 || parts[0] == "" {
		return nil, fmt.Errorf("expected user:password, got %q", s)
	}
	return &Credentials{User: parts[0], Pass: parts[1]}, nil
}

func (c *Credentials) Check(r *http.Request) bool {
	auth := r.Header.Get("Proxy-Authorization")
	if auth == "" {
		return false
	}
	const prefix = "Basic "
	if !strings.HasPrefix(auth, prefix) {
		return false
	}
	decoded, err := base64.StdEncoding.DecodeString(auth[len(prefix):])
	if err != nil {
		return false
	}
	parts := strings.SplitN(string(decoded), ":", 2)
	if len(parts) != 2 {
		return false
	}
	return parts[0] == c.User && parts[1] == c.Pass
}
