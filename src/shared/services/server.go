package services

import (
	"context"
	"crypto/tls"
	"fmt"
	"net"
	"net/http"
	"os"
	"os/signal"
	"sync"
	"syscall"
	"time"

	"pixielabs.ai/pixielabs/src/shared/services/env"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"
)

// PLServer is the services server component used across all Pixie Labs services.
// It starts both an HTTP and a GRPC server and handles middelware and env injection.
type PLServer struct {
	ch          chan bool
	wg          *sync.WaitGroup
	grpcServer  *grpc.Server
	httpHandler http.Handler
	httpServer  *http.Server
}

// NewPLServer creates a new PLServer.
func NewPLServer(env env.Env, httpHandler http.Handler) *PLServer {
	s := &PLServer{
		ch:          make(chan bool),
		wg:          &sync.WaitGroup{},
		grpcServer:  CreateGRPCServer(env),
		httpHandler: httpHandler,
	}
	return s
}

// GRPCServer returns a pointer to the underlying GRPC server.
func (s *PLServer) GRPCServer() *grpc.Server {
	return s.grpcServer
}

func (s *PLServer) serveGRPC() {
	s.wg.Add(1)
	defer s.wg.Done()

	serverAddr := fmt.Sprintf(":%d", viper.GetInt("grpc_port"))
	log.WithField("addr", serverAddr).Infof("Starting GRPC server")
	lis, _ := net.Listen("tcp", serverAddr)

	// Register GRPC reflection.
	reflection.Register(s.grpcServer)
	if err := s.grpcServer.Serve(lis); err != nil {
		log.WithError(err).Fatal("Failed to serve GRPC.")
	}
	log.Info("GRPC server stopped.")
}

func (s *PLServer) serveHTTP() {
	sslEnabled := !viper.GetBool("disable_ssl")
	s.wg.Add(1)
	defer s.wg.Done()
	var tlsConfig *tls.Config

	if sslEnabled {
		var err error
		tlsConfig, err = DefaultServerTLSConfig()
		if err != nil {
			log.WithError(err).Fatal("Failed to load default server TLS config")
		}
	}

	wrappedHandler := HTTPLoggingMiddleware(s.httpHandler)
	serverAddr := fmt.Sprintf(":%d", viper.GetInt("http_port"))
	log.WithField("addr", serverAddr).Print("Starting HTTP server")
	server := &http.Server{
		Addr:           serverAddr,
		Handler:        wrappedHandler,
		TLSConfig:      tlsConfig,
		ReadTimeout:    1800 * time.Second,
		WriteTimeout:   1800 * time.Second,
		MaxHeaderBytes: 1 << 20,
	}
	lis, err := net.Listen("tcp", serverAddr)
	if err != nil {
		log.WithError(err).Fatal("Failed to listen")
	}
	s.httpServer = server
	if sslEnabled {
		lis = tls.NewListener(lis, server.TLSConfig)
	}
	if err := server.Serve(lis); err != nil {
		// Check for graceful termination.
		if err != http.ErrServerClosed {
			log.WithError(err).Fatal("Failed to run HTTP server")
		}
	}
	log.Info("HTTP server stopped.")
}

// Start runs the services in go routines. It returns immediately.
// On error in starting services the program will terminate.
func (s *PLServer) Start() {
	go s.serveHTTP()
	go s.serveGRPC()
}

// Stop will gracefully shutdown underlying GRPC and HTTP servers.
func (s *PLServer) Stop() {
	log.Info("Stopping servers.")
	if s.httpServer != nil {
		go func() {
			ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
			defer cancel()
			if err := s.httpServer.Shutdown(ctx); err != nil {
				log.WithError(err).Fatal("Failed to do a graceful shutdown of HTTP server.")
			}
		}()
	}
	if s.grpcServer != nil {
		go s.grpcServer.Stop()
	}
	s.wg.Wait()
}

// StopOnInterrupt gracefully shuts down when ctrl-c is pressed or termination signal is received.
func (s *PLServer) StopOnInterrupt() {
	ch := make(chan os.Signal)
	signal.Notify(ch, syscall.SIGINT, syscall.SIGTERM)
	<-ch
	s.Stop()
}
