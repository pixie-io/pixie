package controller

import (
	"context"
	"io/ioutil"
	"path/filepath"

	"github.com/spf13/viper"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"github.com/spf13/pflag"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
)

func init() {
	pflag.String("vizier_image_secret_path", "/vizier-image-secret", "[WORKAROUND] The path the the image secrets")
	pflag.String("vizier_image_secret_file", "vizier_image_secret.json", "[WORKAROUND] The image secret file")
}

// VizierImageAuthSever is the GRPC server responsible for providing access to Vizier images.
type VizierImageAuthSever struct{}

// GetImageCredentials fetches image credentials for vizier.
func (v VizierImageAuthSever) GetImageCredentials(context.Context, *cloudapipb.GetImageCredentialsRequest) (*cloudapipb.GetImageCredentialsResponse, error) {
	// TODO(zasgar/michelle): Fix this to create creds for user.
	// This is a workaround implementation to just give them access based on static keys.
	p := viper.GetString("vizier_image_secret_path")
	f := viper.GetString("vizier_image_secret_file")

	absP, err := filepath.Abs(p)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to parse creds paths")
	}
	credsFile := filepath.Join(absP, f)
	b, err := ioutil.ReadFile(credsFile)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to read creds file")
	}

	return &cloudapipb.GetImageCredentialsResponse{Creds: string(b)}, nil
}
