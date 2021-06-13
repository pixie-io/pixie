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
	"fmt"
	"os"
	"strings"

	"github.com/gogo/protobuf/jsonpb"
	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"gopkg.in/src-d/go-git.v4"
	"gopkg.in/src-d/go-git.v4/plumbing/object"

	vpb "px.dev/pixie/src/shared/artifacts/versionspb"
)

func init() {
	pflag.String("repo_path", "", "Path to the git repo")
	pflag.String("artifact_name", "cli", "The release artifact to check")
	pflag.String("versions_file", "VERSIONS.json", "The versions output json file")
}

func availableArtifacts(artifactName string) []vpb.ArtifactType {
	switch {
	case artifactName == "cli":
		return []vpb.ArtifactType{vpb.AT_LINUX_AMD64, vpb.AT_DARWIN_AMD64}
	case artifactName == "vizier":
		return []vpb.ArtifactType{vpb.AT_CONTAINER_SET_LINUX_AMD64, vpb.AT_CONTAINER_SET_YAMLS, vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS}
	case artifactName == "operator":
		return []vpb.ArtifactType{vpb.AT_CONTAINER_SET_LINUX_AMD64, vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS}
	default:
		panic(fmt.Sprintf("Unknown artifact type: %s", artifactName))
	}
}

func parseTagsIntoVersionFile(repoPath string, artifactName string, outputFile string) error {
	r, err := git.PlainOpen(repoPath)
	if err != nil {
		return err
	}

	as := vpb.ArtifactSet{}
	as.Name = artifactName

	releaseTagPrefix := fmt.Sprintf("release/%s/v", artifactName)
	tags, err := r.TagObjects()
	if err != nil {
		return err
	}

	// This only gets annotated tags, which is what we use in the release process.
	err = tags.ForEach(func(tag *object.Tag) error {
		if !strings.HasPrefix(tag.Name, releaseTagPrefix) {
			return nil
		}

		versionStr := strings.TrimPrefix(tag.Name, releaseTagPrefix)
		tpb, err := types.TimestampProto(tag.Tagger.When)
		if err != nil {
			return err
		}

		artifact := &vpb.Artifact{
			Timestamp:          tpb,
			CommitHash:         tag.Hash.String(),
			VersionStr:         versionStr,
			AvailableArtifacts: availableArtifacts(artifactName),
			Changelog:          tag.Message,
		}

		as.Artifact = append(as.Artifact, artifact)
		return nil
	})
	if err != nil {
		return err
	}

	m := jsonpb.Marshaler{}
	f, err := os.Create(outputFile)
	if err != nil {
		return err
	}
	defer f.Close()

	if err := m.Marshal(f, &as); err != nil {
		return err
	}

	return nil
}

func main() {
	pflag.Parse()

	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	path := viper.GetString("repo_path")
	artifactName := viper.GetString("artifact_name")
	versionsFile := viper.GetString("versions_file")

	if len(path) == 0 {
		log.Fatalln("Repo path (--repo_path) is required")
	}

	log.WithField("path", path).Info("Open GIT")
	log.WithField("name", artifactName).Info("Artifact")
	log.WithField("file", versionsFile).Info("Output File")

	if err := parseTagsIntoVersionFile(path, artifactName, versionsFile); err != nil {
		log.Fatalln(err)
	}
}
