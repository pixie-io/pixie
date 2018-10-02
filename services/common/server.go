package common

import (
	"crypto/tls"
	"fmt"
	"net"
	"net/http"
	"strings"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
)

// GrpcHandlerFunc returns an http.Handler that delegates to grpcServer on incoming gRPC
// connections or otherHandler otherwise. Borrowed from cockroachdb.
func GrpcHandlerFunc(grpcServer *grpc.Server, otherHandler http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.ProtoMajor == 2 && strings.Contains(r.Header.Get("Content-Type"), "application/grpc") {
			grpcServer.ServeHTTP(w, r)
		} else {
			otherHandler.ServeHTTP(w, r)
		}
	})
}

// CreateAndRunTLSServer runs a TLS enabled HTTP server.
func CreateAndRunTLSServer(handler http.Handler) {
	tlsCert := viper.GetString("tls_cert")
	tlsKey := viper.GetString("tls_key")

	log.WithFields(log.Fields{
		"tlsCertFile": tlsCert,
		"tlsKeyFile":  tlsKey,
	}).Info("Loading certs")

	// Setup key pairs.
	pair, err := tls.LoadX509KeyPair(tlsCert, tlsKey)
	if err != nil {
		log.WithError(err).Fatal("Failed to load certs.")
	}
	serverAddr := fmt.Sprintf(":%d", viper.GetInt("port"))
	log.WithField("addr", serverAddr).Print("Starting HTTP server")

	s := &http.Server{
		Addr:    serverAddr,
		Handler: handler,
		TLSConfig: &tls.Config{
			Certificates: []tls.Certificate{pair},
			NextProtos:   []string{"h2"},
		},
		ReadTimeout:    1800 * time.Second,
		WriteTimeout:   1800 * time.Second,
		MaxHeaderBytes: 1 << 20,
	}
	conn, _ := net.Listen("tcp", serverAddr)
	log.Panic(s.Serve(tls.NewListener(conn, s.TLSConfig)))
}
