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

package yamls

import (
	"archive/tar"
	"errors"
	"fmt"
	"os"
	"path"
)

// ExtractYAMLFormat represents the types of formats we can extract YAMLs to.
type ExtractYAMLFormat int

const (
	// UnknownExtractYAMLFormat is an extraction format.
	UnknownExtractYAMLFormat ExtractYAMLFormat = iota
	// SingleFileExtractYAMLFormat extracts YAMLs to single file.
	SingleFileExtractYAMLFormat
	// MultiFileExtractYAMLFormat extract YAMLs into multiple files, according to type.
	MultiFileExtractYAMLFormat
)

// YAMLFile is a YAML associated with a name.
type YAMLFile struct {
	Name string
	YAML string
}

// ConcatYAMLs concatenates two separate YAMLs.
func ConcatYAMLs(y1 string, y2 string) string {
	return y1 + "---\n" + y2
}

// ExtractYAMLs writes the generated YAMLs to a tar at the given path in the given format.
func ExtractYAMLs(yamls []*YAMLFile, extractPath string, yamlDir string, format ExtractYAMLFormat) error {
	writeYAML := func(w *tar.Writer, name string, contents string) error {
		if err := w.WriteHeader(&tar.Header{Name: name, Size: int64(len(contents)), Mode: 0777}); err != nil {
			return err
		}
		if _, err := w.Write([]byte(contents)); err != nil {
			return err
		}
		return nil
	}

	filePath := path.Join(extractPath, "yamls.tar")
	writer, err := os.OpenFile(filePath, os.O_RDWR|os.O_CREATE, 0755)
	if err != nil {
		return fmt.Errorf("Failed trying  to open extract_yaml path: %s", err)
	}
	defer writer.Close()
	w := tar.NewWriter(writer)

	switch format {
	case MultiFileExtractYAMLFormat:
		for i, y := range yamls {
			err = writeYAML(w, fmt.Sprintf("./%s/%02d_%s.yaml", yamlDir, i, y.Name), yamls[i].YAML)
			if err != nil {
				return err
			}
		}
	case SingleFileExtractYAMLFormat:
		// Combine all YAMLs into a single file.
		combinedYAML := ""
		for _, y := range yamls {
			combinedYAML = ConcatYAMLs(combinedYAML, y.YAML)
		}
		err = writeYAML(w, fmt.Sprintf("./%s/manifest.yaml", yamlDir), combinedYAML)
		if err != nil {
			return err
		}
	default:
		return errors.New("Invalid extract YAML format")
	}

	if err = w.Close(); err != nil {
		if err != nil {
			return errors.New("Failed to write YAMLs")
		}
	}
	return nil
}
