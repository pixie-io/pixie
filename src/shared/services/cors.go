package services

import (
	"github.com/gorilla/handlers"
)

// DefaultCORSConfig has the default config setup for CORS.
func DefaultCORSConfig(allowedOrigins []string) []handlers.CORSOption {
	return []handlers.CORSOption{
		handlers.AllowedMethods([]string{"POST", "OPTIONS"}),
		handlers.AllowedHeaders([]string{"Content-Type", "Origin", "Accept", "token", "authorization"}),
		handlers.AllowedOrigins(allowedOrigins),
	}
}
