/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package controllers

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
