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
	"bufio"
	"context"
	"encoding/base64"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"strings"
	"sync"
	"time"

	"github.com/PuerkitoBio/goquery"
	"github.com/blang/semver"
	"github.com/google/go-github/v32/github"
	"golang.org/x/oauth2"
)

var (
	modules           = flag.String("modules", "", "Path to the a file listing all go module deps")
	tryPkgDevGo       = flag.Bool("try_pkg_dev_go", false, "Whether to query pkg.dev.go for licenses")
	fatalIfMissing    = flag.Bool("fatal_if_missing", false, "Whether to treat any missing dependecies as a fatal error")
	jsonManualInput   = flag.String("json_manual_input", "", "Path to input json file with manually fetched licenses")
	jsonOutput        = flag.String("json_output", "", "Path to output json file")
	jsonMissingOutput = flag.String("json_missing_output", "", "Path to output json file with missing licenses")
)

var remapRepos = map[string]string{
	"pixie-io/arrow":        "apache/arrow",
	"pixie-io/bcc":          "iovisor/bcc",
	"pixie-io/bpftrace":     "iovisor/bpftrace",
	"pixie-io/cpplint":      "cpplint/cpplint",
	"pixie-io/dnsparser":    "packetzero/dnsparser",
	"pixie-io/ELFIO":        "serge1/ELFIO",
	"pixie-io/grpc":         "grpc/grpc",
	"pixie-io/jattach":      "apangin/jattach",
	"pixie-io/libpypa":      "vinzenz/libpypa",
	"pixie-io/tdigest":      "derrickburns/tdigest",
	"pixie-io/threadstacks": "thoughtspot/threadstacks",
}

// Keep this in sync with src/ui/tools/licenses/yarn_license_extractor.js
type dependency struct {
	Name        string `json:"name"`
	URL         string `json:"url"`
	Package     string `json:"-"`
	Version     string `json:"version"`
	LicenseSPDX string `json:"spdxID,omitempty"`
	LicenseText string `json:"licenseText,omitempty"`
	// When a manual license is needed, which doesn't use a github URL, you should:
	//  set Name to URL, and OutputName to the desired name.
	OutputName string `json:"outputName,omitempty"`
}

func readData(filename string) (map[string]dependency, error) {
	licenseFile, err := os.Open(filename)
	if err != nil {
		return nil, err
	}
	defer licenseFile.Close()

	var deps []dependency
	dec := json.NewDecoder(licenseFile)
	err = dec.Decode(&deps)
	if err != nil && err != io.EOF {
		return nil, err
	}

	licenses := make(map[string]dependency)
	for _, dep := range deps {
		licenses[dep.Name] = dep
	}
	return licenses, nil
}

func writeData(dependencies []*dependency, filename string) error {
	outputFile, err := os.Create(filename)
	if err != nil {
		return err
	}
	defer outputFile.Close()
	jsonStr, err := json.MarshalIndent(dependencies, "", "    ")
	if err != nil {
		return err
	}
	if _, err := outputFile.Write(jsonStr); err != nil {
		return err
	}
	return outputFile.Sync()
}

func parseDep(line string) *dependency {
	dep := &dependency{Package: line}

	if *tryPkgDevGo {
		parts := strings.Split(line, " ")
		if len(parts) > 1 {
			dep.Package = parts[0]
			dep.Version = strings.TrimSuffix(parts[1], "/go.mod")
		}
	}

	dep.Name = dep.Package

	if strings.HasPrefix(line, "#manual-license-name:") {
		dep.Name = strings.TrimPrefix(line, "#manual-license-name:")
	}

	if *tryPkgDevGo && !strings.Contains(dep.Package, "github") {
		// This is a non github go pkg dep. Resolve to the go pkg manager
		// since these are usually URL like but not always valid pages on the internet.
		// e.g. k8s.io/klog
		dep.URL = fmt.Sprintf("https://pkg.go.dev/%s", dep.Package)
		return dep
	}

	if !strings.Contains(dep.Package, "github") {
		// At this point we better be in Github land else all assumptions break.
		dep.URL = ""
		return dep
	}

	// Drop the scheme if any.
	if strings.Contains(dep.Name, "://") {
		parts := strings.Split(dep.Name, "://")
		dep.Name = parts[1]
	}

	// Drop anything besides the Username and Reponame.
	parts := strings.Split(dep.Name, "/")
	if len(parts) < 3 {
		// This dep is somehow underspecified?
		dep.URL = ""
		return dep
	}

	dep.Name = strings.Join(parts[1:3], "/")
	// Remap our forks to the orignal.
	if _, ok := remapRepos[dep.Name]; ok {
		dep.Name = remapRepos[dep.Name]
	}

	if strings.HasSuffix(parts[len(parts)-1], ".tar.gz") {
		dep.Version = strings.TrimSuffix(parts[len(parts)-1], ".tar.gz")
	}
	if strings.HasSuffix(parts[len(parts)-1], ".zip") {
		dep.Version = strings.TrimSuffix(parts[len(parts)-1], ".zip")
	}
	if len(parts) > 4 && parts[len(parts)-2] == "commit" {
		dep.Version = parts[len(parts)-1]
	}

	dep.URL = fmt.Sprintf("https://github.com/%s", dep.Name)
	return dep
}

func tryFetchGithubLicense(ctx context.Context, client *github.Client, dep *dependency) {
	if !strings.HasPrefix(dep.URL, "https://github.com") {
		// Can't fetch licenses from GitHub if this is not on GitHub.
		return
	}
	parts := strings.Split(dep.Name, "/")
	if len(parts) < 2 {
		return
	}
	repoLicense, _, err := client.Repositories.License(ctx, parts[0], parts[1])
	if err != nil {
		return
	}
	dep.LicenseSPDX = repoLicense.GetLicense().GetSPDXID()
	if dep.LicenseSPDX == "NOASSERTION" {
		// GitHub found some license text but didn't map it to a SPDX ID.
		// Let someone else figure this out.
		dep.LicenseSPDX = ""
	}
	// GitHub gives us base64 encoded licenses ... hooray.
	decodedLicense, err := base64.StdEncoding.DecodeString(repoLicense.GetContent())
	if err != nil {
		dep.LicenseText = repoLicense.GetContent()
	} else {
		dep.LicenseText = string(decodedLicense)
	}
}

func tryFetchPkgGoDevLicense(ctx context.Context, dep *dependency) {
	for retryCount := 0; retryCount < 5; retryCount++ {
		if !*tryPkgDevGo {
			// Must not be go deps.
			return
		}
		if dep.LicenseSPDX != "" {
			// We must have fetched from GitHub successfully, so don't do anything here.
			return
		}
		resp, err := http.Get(fmt.Sprintf("https://pkg.go.dev/%s?tab=licenses", dep.Package))
		if err != nil {
			return
		}
		defer resp.Body.Close()
		if resp.StatusCode == 429 {
			time.Sleep(1 << retryCount * time.Second)
			continue
		}
		if resp.StatusCode != 200 {
			return
		}
		doc, err := goquery.NewDocumentFromReader(resp.Body)
		if err != nil {
			time.Sleep(1 << retryCount * time.Second)
			continue
		}
		// Let's hope that the HTML structure of this page never changes ... because,
		// they somehow still don't have an API to access pkg.go.dev
		doc.Find(".License").Each(func(_ int, s *goquery.Selection) {
			if dep.LicenseSPDX != "" {
				return
			}
			dep.LicenseSPDX = s.Find("h2 div").Text()
			dep.LicenseText = s.Find(".License-contents").Text()
		})
	}
}

func tryFetchJSONManualLicense(dep *dependency, manual map[string]dependency) {
	if dep.LicenseSPDX != "" {
		// A previous fetcher was successful, so don't do anything here.
		return
	}
	found, ok := manual[dep.Name]
	if !ok {
		// Well, at this point someone needs to go add license info for this into
		// the manual_licenses.json file.
		return
	}
	if found.OutputName != "" {
		dep.Name = found.OutputName
	}
	if found.Version != "" {
		dep.Version = found.Version
	}
	if dep.URL == "" {
		dep.URL = found.URL
	}
	dep.LicenseSPDX = found.LicenseSPDX
	dep.LicenseText = found.LicenseText
}

func mustParseTolerant(s string) semver.Version {
	v, err := semver.ParseTolerant(s)
	if err != nil {
		panic(`semver: Parse(` + s + `): ` + err.Error())
	}
	return v
}

func main() {
	flag.Parse()
	ctx := context.Background()

	if *modules == "" {
		log.Fatal("Must specfy --modules")
	}

	if *jsonOutput == "" {
		log.Fatal("Must specfy --json_output")
	}

	var manual map[string]dependency
	var err error
	if *jsonManualInput != "" {
		manual, err = readData(*jsonManualInput)
	}
	if err != nil {
		log.Fatal(err)
	}

	modulesFile, err := os.Open(*modules)
	if err != nil {
		log.Fatal(err)
	}
	defer modulesFile.Close()

	depsByName := make(map[string]*dependency)
	scanner := bufio.NewScanner(modulesFile)
	for scanner.Scan() {
		dep := parseDep(scanner.Text())
		if depsByName[dep.Name] == nil {
			depsByName[dep.Name] = dep
			continue
		}
		if mustParseTolerant(dep.Version).GT(mustParseTolerant(depsByName[dep.Name].Version)) {
			depsByName[dep.Name] = dep
			continue
		}
	}
	if err := scanner.Err(); err != nil {
		log.Fatal(err)
	}

	deps := make([]*dependency, len(depsByName))
	i := 0
	for _, dep := range depsByName {
		deps[i] = dep
		i++
	}

	var tc *http.Client
	githubToken, ok := os.LookupEnv("GH_API_KEY")
	if ok && githubToken != "" {
		tc = oauth2.NewClient(ctx, oauth2.StaticTokenSource(&oauth2.Token{AccessToken: githubToken}))
	}
	client := github.NewClient(tc)

	var wg sync.WaitGroup
	work := make(chan *dependency)

	for i := 0; i < 12; i++ {
		wg.Add(1)
		go func(ctx context.Context, client *github.Client, manual map[string]dependency) {
			defer wg.Done()
			for dep := range work {
				tryFetchGithubLicense(ctx, client, dep)
				tryFetchPkgGoDevLicense(ctx, dep)
				tryFetchJSONManualLicense(dep, manual)
			}
		}(ctx, client, manual)
	}

	for _, dep := range deps {
		work <- dep
	}

	close(work)
	wg.Wait()

	// Create slices instead of leaving this as nil so that
	// JSON marshal will write [] into the output instead of null.
	found := make([]*dependency, 0)
	missing := make([]*dependency, 0)

	for _, dep := range deps {
		if dep.LicenseSPDX != "" {
			found = append(found, dep)
		} else {
			missing = append(missing, dep)
		}
	}

	err = writeData(found, *jsonOutput)
	if err != nil {
		log.Fatal(err)
	}
	if *jsonMissingOutput != "" {
		err = writeData(missing, *jsonMissingOutput)
		if err != nil {
			log.Fatal(err)
		}
	}
	if *fatalIfMissing && len(missing) > 0 {
		log.Fatalf("There are %d repos with missing licenses and --fatal_if_missing was set", len(missing))
	}
}
