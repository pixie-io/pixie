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
	"io"
	"net/http"
	"os"
	"strings"

	"github.com/gogo/protobuf/jsonpb"
	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"gopkg.in/src-d/go-git.v4"
	"gopkg.in/src-d/go-git.v4/plumbing"
	"gopkg.in/yaml.v3"

	vpb "px.dev/pixie/src/shared/artifacts/versionspb"
)

func init() {
	pflag.String("repo_path", "", "Path to the git repo")
	pflag.String("artifact_name", "cli", "The release artifact to check")
	pflag.String("versions_file", "VERSIONS.json", "The versions output json file")
	pflag.String("mirrors_file", "", "Path to artifact_mirrors file to backfill availableArtifactMirrors."+
		"If empty, versions file will use AvailableArtifacts instead of AvailableArtifactMirrors.")
	pflag.String("gh_repo_name", "pixie-io/pixie", "Github owner/repo to use for release URLs")
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

func getSHA(url string) (string, error) {
	resp, err := http.Get(url)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("received non-OK http code: %s", resp.Status)
	}
	bytes, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	return strings.TrimSpace(string(bytes)), nil
}

func availableArtifactMirrors(ghRepo string, component string, version string, mirrors []*mirror) []*vpb.ArtifactMirrors {
	artifacts := make(map[string]vpb.ArtifactType)
	switch component {
	case "cli":
		artifacts["cli_darwin_amd64"] = vpb.AT_DARWIN_AMD64
		artifacts["cli_linux_amd64"] = vpb.AT_LINUX_AMD64
	case "vizier":
		artifacts["vizier_yamls.tar"] = vpb.AT_CONTAINER_SET_YAMLS
		artifacts["vizier_template_yamls.tar"] = vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS
	case "operator":
		artifacts["operator_template_yamls.tar"] = vpb.AT_CONTAINER_SET_TEMPLATE_YAMLS
	}

	var ams []*vpb.ArtifactMirrors
	for name, at := range artifacts {
		am := &vpb.ArtifactMirrors{
			ArtifactType: at,
		}
		for _, m := range mirrors {
			url := strings.ReplaceAll(m.URLFormat, "${component}", component)
			url = strings.ReplaceAll(url, "${version}", version)
			url = strings.ReplaceAll(url, "${artifact_name}", name)
			url = strings.ReplaceAll(url, "${gh_repo}", ghRepo)
			shaURL := url + ".sha256"

			log.WithField("url", url).Info("Trying mirror")
			sha, err := getSHA(shaURL)
			if err != nil {
				continue
			}
			am.SHA256 = sha
			am.URLs = append(am.URLs, url)
		}
		if am.SHA256 != "" && len(am.URLs) > 0 {
			ams = append(ams, am)
		}
	}
	return ams
}

type mirror struct {
	URLFormat string `yaml:"url_format"`
}

func parseMirrorsFile(path string) ([]*mirror, error) {
	f, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	dec := yaml.NewDecoder(f)
	mirrors := []*mirror{}
	if err := dec.Decode(&mirrors); err != nil {
		return nil, err
	}
	return mirrors, nil
}

func parseTagsIntoVersionFile(ghRepoName string, repoPath string, artifactName string, outputFile string, mirrorsFile string) error {
	r, err := git.PlainOpen(repoPath)
	if err != nil {
		return err
	}

	mirrors := []*mirror{}
	if mirrorsFile != "" {
		mirrors, err = parseMirrorsFile(mirrorsFile)
		if err != nil {
			return err
		}
	}

	as := vpb.ArtifactSet{}
	as.Name = artifactName

	releaseTagPrefix := fmt.Sprintf("release/%s/v", artifactName)

	// TagObjects returns all annotated tag objects regardless if an actual tag ref currently points to them.
	// Instead of getting annotated tags via TagObjects, we iterate all the actual tag references
	// and then skip the ones that don't have an associated TagObject.
	tags, err := r.Tags()
	if err != nil {
		return err
	}

	err = tags.ForEach(func(ref *plumbing.Reference) error {
		tag, err := r.TagObject(ref.Hash())
		switch err {
		case nil:
		case plumbing.ErrObjectNotFound:
			return nil
		default:
			return err
		}

		if !strings.HasPrefix(tag.Name, releaseTagPrefix) {
			return nil
		}

		versionStr := strings.TrimPrefix(tag.Name, releaseTagPrefix)
		tpb, err := types.TimestampProto(tag.Tagger.When)
		if err != nil {
			return err
		}

		artifact := &vpb.Artifact{
			Timestamp:  tpb,
			CommitHash: tag.Target.String(),
			VersionStr: versionStr,
			Changelog:  tag.Message,
		}
		if len(mirrors) == 0 {
			artifact.AvailableArtifacts = availableArtifacts(artifactName)
		} else {
			artifact.AvailableArtifactMirrors = availableArtifactMirrors(ghRepoName, artifactName, versionStr, mirrors)
		}

		if len(artifact.AvailableArtifacts) > 0 || len(artifact.AvailableArtifactMirrors) > 0 {
			as.Artifact = append(as.Artifact, artifact)
		}
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
	mirrorsFile := viper.GetString("mirrors_file")
	ghRepoName := viper.GetString("gh_repo_name")

	if len(path) == 0 {
		log.Fatalln("Repo path (--repo_path) is required")
	}

	log.WithField("path", path).Info("Open GIT")
	log.WithField("name", artifactName).Info("Artifact")
	log.WithField("file", versionsFile).Info("Output File")

	if err := parseTagsIntoVersionFile(ghRepoName, path, artifactName, versionsFile, mirrorsFile); err != nil {
		log.Fatalln(err)
	}
}
