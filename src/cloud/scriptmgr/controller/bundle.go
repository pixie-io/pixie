package controller

import (
	"context"
	"encoding/json"

	"github.com/googleapis/google-cloud-go-testing/storage/stiface"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

/*
NOTE(james): This file is temporary. We will eventually move away from using bundle.json,
to store scripts.
*/

type pixieScript struct {
	Pxl       string `json:"pxl"`
	Vis       string `json:"vis"`
	Placement string `json:"placement"`
	ShortDoc  string `json:"ShortDoc"`
	LongDoc   string `json:"LongDoc"`
}

type bundle struct {
	Scripts map[string]*pixieScript `json:"scripts"`
}

func getBundle(sc stiface.Client, bundleBucket string, bundlePath string) (*bundle, error) {
	ctx := context.Background()
	r, err := sc.Bucket(bundleBucket).Object(bundlePath).NewReader(ctx)
	if err != nil {
		return nil, status.Error(codes.Internal, "failed to download bundle.json")
	}
	defer r.Close()

	var b bundle
	err = json.NewDecoder(r).Decode(&b)
	if err != nil {
		return nil, err
	}
	return &b, nil
}
