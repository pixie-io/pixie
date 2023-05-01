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

package update

import (
	"context"
	"encoding/hex"
	"io"
	"net/http"
	"os"
	"runtime"
	"strconv"
	"strings"

	"github.com/blang/semver"
	"github.com/inconshreveable/go-update"
	"github.com/kardianos/osext"
	"github.com/vbauerster/mpb/v4"
	"github.com/vbauerster/mpb/v4/decor"
	"golang.org/x/sys/unix"
	"google.golang.org/grpc"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	version "px.dev/pixie/src/shared/goversion"
	"px.dev/pixie/src/shared/services"
)

func newATClient(cloudAddr string) (cloudpb.ArtifactTrackerClient, error) {
	isInternal := strings.Contains(cloudAddr, "cluster.local")

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return nil, err
	}

	c, err := grpc.Dial(cloudAddr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return cloudpb.NewArtifactTrackerClient(c), nil
}

func getArtifactTypes() cloudpb.ArtifactType {
	switch runtime.GOOS {
	case "darwin":
		return cloudpb.AT_DARWIN_AMD64
	case "linux":
		return cloudpb.AT_LINUX_AMD64
	}
	return cloudpb.AT_UNKNOWN
}

// UpdatesAvailable returns the version if updates are available, otherwise empty string.
// Errors also return empty strings.
func UpdatesAvailable(cloudAddr string) string {
	u := NewCLIUpdater(cloudAddr)
	versions, err := u.GetAvailableVersions(version.GetVersion().Semver())
	if err != nil {
		return ""
	}
	if version.GetVersion().IsDev() {
		return ""
	}
	if len(versions) == 0 {
		return ""
	}
	return versions[0]
}

// CLIUpdater manages updates to the CLI.
type CLIUpdater struct {
	cloudAddr string
}

// NewCLIUpdater creates a new CLIUpdater.
func NewCLIUpdater(cloudAddr string) *CLIUpdater {
	return &CLIUpdater{
		cloudAddr: cloudAddr,
	}
}

// GetAvailableVersions returns a list (max 10) of available versions > specified version.
func (c *CLIUpdater) GetAvailableVersions(minVersion semver.Version) ([]string, error) {
	req := cloudpb.GetArtifactListRequest{
		ArtifactName: "cli",
		ArtifactType: getArtifactTypes(),
		Limit:        10,
	}

	client, err := newATClient(c.cloudAddr)
	if err != nil {
		return nil, err
	}

	resp, err := client.GetArtifactList(context.Background(), &req)
	if err != nil {
		return nil, err
	}

	versionList := make([]string, 0)
	for _, art := range resp.Artifact {
		v := strings.TrimPrefix(art.VersionStr, "v")
		// FIXME: remove me before checkin.
		if !strings.HasPrefix(v, "0") {
			continue
		}
		version := semver.MustParse(v)
		if minVersion.LT(version) {
			versionList = append(versionList, art.VersionStr)
		}
	}
	return versionList, nil
}

// IsUpdatable checks file permissions to make sure that the CLI can be updated.
func (c *CLIUpdater) IsUpdatable() (bool, error) {
	executablePath, err := osext.Executable()
	if err != nil {
		return false, err
	}
	err = unix.Access(executablePath, unix.W_OK)
	if err == nil {
		return true, nil
	}
	// File is not writable, check if the current user is owner.
	s := &unix.Stat_t{}
	err = unix.Stat(executablePath, s)
	if err != nil {
		return false, err
	}
	if int(s.Uid) != os.Getuid() {
		return false, nil
	}
	return true, nil
}

// UpdateSelf updates the CLI to the specified version.
func (c *CLIUpdater) UpdateSelf(version string) error {
	req := cloudpb.GetDownloadLinkRequest{
		ArtifactName: "cli",
		ArtifactType: getArtifactTypes(),
		VersionStr:   version,
	}

	client, err := newATClient(c.cloudAddr)
	if err != nil {
		return err
	}

	resp, err := client.GetDownloadLink(context.Background(), &req)
	if err != nil {
		return err
	}

	tempFile, err := os.CreateTemp("", "cli_download")
	if err != nil {
		return err
	}
	defer func() {
		_ = os.Remove(tempFile.Name())
	}()

	downloader := newDownloadWithProgress(resp.Url, tempFile.Name())
	err = downloader.Download()
	if err != nil {
		return err
	}

	utils.Info("Download complete, applying update ...")
	checksum, err := hex.DecodeString(resp.SHA256)
	if err != nil {
		return err
	}

	f, err := os.Open(tempFile.Name())
	if err != nil {
		return err
	}

	err = update.Apply(f, update.Options{
		Checksum: checksum,
	})

	return err
}

type downloadWithProgress struct {
	url      string
	savePath string
}

func newDownloadWithProgress(url, savePath string) *downloadWithProgress {
	return &downloadWithProgress{
		url:      url,
		savePath: savePath,
	}
}

func (d *downloadWithProgress) getFileSize() (int64, error) {
	hr, err := http.Head(d.url)
	if err != nil {
		return 0, err
	}
	fileSize, err := strconv.Atoi(hr.Header.Get("Content-Length"))
	if err != nil {
		return 0, err
	}
	return int64(fileSize), err
}

func (d *downloadWithProgress) Download() error {
	fileSize, err := d.getFileSize()
	if err != nil {
		return err
	}

	m := mpb.New()
	name := "Download"
	bar := m.AddBar(int64(fileSize),
		mpb.BarRemoveOnComplete(),
		mpb.PrependDecorators(
			decor.Name(name, decor.WC{W: len(name) + 1, C: decor.DidentRight}),
			decor.OnComplete(
				decor.CountersKibiByte("% .2f / % .2f"),
				"",
			),
			decor.OnComplete(
				decor.EwmaETA(decor.ET_STYLE_GO, 60, decor.WC{W: 4}), "done",
			),
		),
		mpb.AppendDecorators(
			decor.OnComplete(decor.Percentage(), ""),
		),
	)

	f, err := os.Create(d.savePath)
	if err != nil {
		return err
	}

	resp, err := http.Get(d.url)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	p := bar.ProxyReader(resp.Body)
	_, err = io.Copy(f, p)
	if err != nil {
		return err
	}
	m.Wait()
	return nil
}
