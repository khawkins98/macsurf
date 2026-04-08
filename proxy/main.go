package main

import (
	"context"
	"flag"
	"fmt"
	"log"
	"net"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"
)

func main() {
	port := flag.Int("port", 8765, "port to listen on")
	auth := flag.String("auth", "", "basic auth credentials (user:password)")
	flag.Parse()

	var authCreds *Credentials
	if *auth != "" {
		creds, err := ParseCredentials(*auth)
		if err != nil {
			log.Fatalf("invalid --auth format: %v", err)
		}
		authCreds = creds
	}

	proxy := &Proxy{Auth: authCreds}

	server := &http.Server{
		Addr:         fmt.Sprintf(":%d", *port),
		Handler:      proxy,
		ReadTimeout: 30 * time.Second,
		BaseContext: func(_ net.Listener) context.Context {
			return context.Background()
		},
	}

	ctx, stop := signal.NotifyContext(context.Background(), syscall.SIGINT, syscall.SIGTERM)
	defer stop()

	go func() {
		log.Printf("macsurf-proxy listening on :%d", *port)
		if authCreds != nil {
			log.Printf("basic auth enabled for user %q", authCreds.User)
		}
		if err := server.ListenAndServe(); err != http.ErrServerClosed {
			log.Fatalf("server error: %v", err)
		}
	}()

	<-ctx.Done()
	log.Println("shutting down...")

	shutdownCtx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	if err := server.Shutdown(shutdownCtx); err != nil {
		log.Printf("shutdown error: %v", err)
		os.Exit(1)
	}
	log.Println("stopped")
}
