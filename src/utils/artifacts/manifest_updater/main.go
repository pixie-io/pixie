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
	"bytes"
	"context"
	"crypto/sha256"
	"errors"
	"fmt"
	"net/http"
	"os"
	"path/filepath"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/shared/artifacts/manifest"

	"cloud.google.com/go/storage"
	"google.golang.org/api/googleapi"
)

func init() {
	pflag.String("manifest_bucket", "", "GCS Bucket where manifest is stored")

	pflag.String("manifest_path", "", "Path within bucket/local path of manifest file")
	pflag.String("manifest_updates", "", "Path to json file with updates for manifest")
	pflag.Int("num_retries", 10, "Number of times to retry on conflict")

	viper.SetEnvPrefix("PX_")
	viper.AutomaticEnv()
	pflag.Parse()
	viper.BindPFlags(pflag.CommandLine)
}

type updateManifestFunc func(*manifest.ArtifactManifest) error

func main() {
	var updateManifest updateManifestFunc
	if viper.GetString("manifest_bucket") == "" {
		updateManifest = updateLocalManifest
	} else {
		updateManifest = updateGCSManifest
	}
	updates, err := getUpdates()
	if err != nil {
		log.WithError(err).Fatal("failed to read manifest updates")
	}
	for i := 0; i < viper.GetInt("num_retries"); i++ {
		err := updateManifest(updates)
		if err == nil {
			return
		}
		if errors.Is(err, errConflict) {
			time.Sleep(5 * time.Second)
			continue
		}
		log.WithError(err).Error("failed to update manifest")
		return
	}
	log.Error("failed to update manifest, too many conflict retries")
}

func getUpdates() (*manifest.ArtifactManifest, error) {
	updatesReader, err := os.Open(viper.GetString("manifest_updates"))
	if err != nil {
		return nil, err
	}
	defer updatesReader.Close()
	updates, err := manifest.ReadArtifactManifest(updatesReader)
	if err != nil {
		return nil, err
	}
	return updates, nil
}

var errConflict = errors.New("manifest upload conflicted with another upload attempt")

func updateGCSManifest(updates *manifest.ArtifactManifest) error {
	ctx := context.Background()
	client, err := storage.NewClient(ctx)
	if err != nil {
		return err
	}

	obj := client.Bucket(viper.GetString("manifest_bucket")).Object(viper.GetString("manifest_path"))

	r, err := obj.NewReader(ctx)
	if err != nil {
		if errors.Is(err, storage.ErrObjectNotExist) {
			return writeGCSManifest(ctx, obj, updates)
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

	if err := writeGCSManifest(ctx, obj, newManifest); err != nil {
		return err
	}
	return nil
}

func writeGCSManifest(ctx context.Context, obj *storage.ObjectHandle, m *manifest.ArtifactManifest) error {
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

func updateLocalManifest(updates *manifest.ArtifactManifest) error {
	path := viper.GetString("manifest_path")

	if path == "" {
		return errors.New("must specify manifest_path to update local manifest")
	}

	curManifest, err := readManifest(path)
	if err != nil {
		return err
	}
	newManifest := curManifest.Merge(updates)

	manifestBuf := &bytes.Buffer{}
	if err := newManifest.Write(manifestBuf); err != nil {
		return err
	}
	if err := os.WriteFile(path, manifestBuf.Bytes(), 0664); err != nil {
		return err
	}

	sha256Bytes := sha256.Sum256(manifestBuf.Bytes())
	sha256Str := fmt.Sprintf("%x", sha256Bytes)
	shaPath := path + ".sha256"
	if err := os.WriteFile(shaPath, []byte(sha256Str), 0664); err != nil {
		return err
	}

	return nil
}

func createEmptyManifest(path string) (*os.File, error) {
	if err := os.MkdirAll(filepath.Dir(path), 0755); err != nil {
		return nil, err
	}
	f, err := os.Create(path)
	if err != nil {
		return nil, err
	}
	if _, err := f.Write([]byte("[]")); err != nil {
		f.Close()
		return nil, err
	}
	if _, err := f.Seek(0, 0); err != nil {
		f.Close()
		return nil, err
	}
	return f, nil
}

func readManifest(path string) (*manifest.ArtifactManifest, error) {
	f, err := os.Open(path)
	if err != nil {
		if os.IsNotExist(err) {
			f, err = createEmptyManifest(path)
			if err != nil {
				return nil, err
			}
		} else {
			return nil, err
		}
	}
	defer f.Close()

	return manifest.ReadArtifactManifest(f)
}
