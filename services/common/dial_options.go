package common

import (
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials"
)

// DefaultClientDialOpts creates the default dialing options for the client.
func DefaultClientDialOpts() ([]grpc.DialOption, error) {
	sslEnabled := !viper.GetBool("disable_ssl")
	dialOpts := make([]grpc.DialOption, 0)
	if sslEnabled {
		creds, err := credentials.NewClientTLSFromFile(viper.GetString("tls_cert"), "")
		if err != nil {
			return nil, err
		}
		dialOpts = append(dialOpts, grpc.WithTransportCredentials(creds))
	} else {
		dialOpts = append(dialOpts, grpc.WithInsecure())
	}
	return dialOpts, nil
}
