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

package main

import (
	"context"
	"errors"
	"net/http"
	"os"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/shared/artifacts/manifest"

	"cloud.google.com/go/storage"
	"google.golang.org/api/googleapi"
)

func init() {
	pflag.String("manifest_bucket", "", "GCS Bucket where manifest is stored")
	pflag.String("manifest_path", "", "Path within bucket of manifest file")
	pflag.String("manifest_updates", "", "Path to json file with updates for manifest")
	pflag.Int("num_retries", 10, "Number of times to retry on conflict")

	viper.SetEnvPrefix("PX_")
	viper.AutomaticEnv()
	pflag.Parse()
	viper.BindPFlags(pflag.CommandLine)
}

func main() {
	for i := 0; i < viper.GetInt("num_retries"); i++ {
		err := downloadAndMergeManifest()
		if err == nil {
			break
		}
		if errors.Is(err, errConflict) {
			continue
		}
		log.WithError(err).Fatal("failed to download and merge manifests")
	}
}

var errConflict = errors.New("manifest upload conflicted with another upload attempt")

func downloadAndMergeManifest() error {
	updatesReader, err := os.Open(viper.GetString("manifest_updates"))
	if err != nil {
		return err
	}
	defer updatesReader.Close()
	updates, err := manifest.ReadArtifactManifest(updatesReader)
	if err != nil {
		return err
	}

	ctx := context.Background()
	client, err := storage.NewClient(ctx)
	if err != nil {
		return err
	}

	obj := client.Bucket(viper.GetString("manifest_bucket")).Object(viper.GetString("manifest_path"))

	r, err := obj.NewReader(ctx)
	if err != nil {
		if errors.Is(err, storage.ErrObjectNotExist) {
			return writeManifest(ctx, obj, updates)
		}
		return err
	}
	defer r.Close()

	curManifest, err := manifest.ReadArtifactManifest(r)
	if err != nil {
		return err
	}

	newManifest := curManifest.Merge(updates)

	r.Close()

	// Make sure that the write only occurs if the manifest hasn't changed since we read it.
	obj = obj.If(storage.Conditions{
		GenerationMatch:     r.Attrs.Generation,
		MetagenerationMatch: r.Attrs.Metageneration,
	})

	if err := writeManifest(ctx, obj, newManifest); err != nil {
		return err
	}
	return nil
}

func writeManifest(ctx context.Context, obj *storage.ObjectHandle, m *manifest.ArtifactManifest) error {
	w := obj.NewWriter(ctx)
	defer w.Close()
	w.ContentType = "application/json"
	w.CacheControl = "no-store, no-cache, max-age=0"
	if err := m.Write(w); err != nil {
		return err
	}
	if err := w.Close(); err != nil {
		switch e := err.(type) {
		case *googleapi.Error:
			if e.Code == http.StatusPreconditionFailed {
				return errConflict
			}
		default:
		}
	}
	return nil
}
