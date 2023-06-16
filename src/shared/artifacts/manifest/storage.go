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

package manifest

import (
	"context"
	"io"
	"net/http"

	"cloud.google.com/go/storage"
)

// Location represents a storage location of a manifest, and allows interacting with it.
type Location interface {
	Checksum(context.Context) ([]byte, error)
	ManifestReader(context.Context) (io.ReadCloser, error)
}

type gcsManifest struct {
	client       *storage.Client
	bucket       string
	manifestPath string
}

// NewGCSLocation creates a new manifest.Location for a manifest stored in GCS.
func NewGCSLocation(client *storage.Client, bucket string, manifestPath string) Location {
	return &gcsManifest{
		client:       client,
		bucket:       bucket,
		manifestPath: manifestPath,
	}
}

func (gcs *gcsManifest) Checksum(ctx context.Context) ([]byte, error) {
	obj := gcs.client.Bucket(gcs.bucket).Object(gcs.manifestPath)

	attrs, err := obj.Attrs(ctx)
	if err != nil {
		return nil, err
	}
	return attrs.MD5, nil
}

func (gcs *gcsManifest) ManifestReader(ctx context.Context) (io.ReadCloser, error) {
	obj := gcs.client.Bucket(gcs.bucket).Object(gcs.manifestPath)
	return obj.NewReader(ctx)
}

type httpManifest struct {
	shaURL      string
	manifestURL string
}

// NewHTTPLocation returns a new Location for a manifest stored at an arbitrary http endpoint.
func NewHTTPLocation(shaURL string, manifestURL string) Location {
	return &httpManifest{
		shaURL:      shaURL,
		manifestURL: manifestURL,
	}
}

func (h *httpManifest) Checksum(ctx context.Context) ([]byte, error) {
	req, err := http.NewRequestWithContext(ctx, "GET", h.shaURL, http.NoBody)
	if err != nil {
		return nil, err
	}

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return nil, err
	}
	defer resp.Body.Close()
	return io.ReadAll(resp.Body)
}

func (h *httpManifest) ManifestReader(ctx context.Context) (io.ReadCloser, error) {
	req, err := http.NewRequestWithContext(ctx, "GET", h.manifestURL, http.NoBody)
	if err != nil {
		return nil, err
	}

	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return nil, err
	}
	return resp.Body, nil
}
