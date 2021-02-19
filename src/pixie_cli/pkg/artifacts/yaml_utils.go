package artifacts

import (
	"archive/tar"
	"bytes"
	"errors"
	"fmt"
	"os"
	"path"
	"text/template"
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

// YAMLTmplValues are the values we can substitue into our YAMLs.
// TODO(michelle): We can probably just use a map[string]interface{} instead of
// explicitly defining all template-able values. This cleanup will come in a followup diff.
type YAMLTmplValues struct {
	DeployKey string
}

// YAMLTmplArguments is a wrapper around YAMLTmplValues.
type YAMLTmplArguments struct {
	Values *YAMLTmplValues
}

// YAMLFile is a YAML associated with a name.
type YAMLFile struct {
	Name string
	YAML string
}

func concatYAMLs(y1 string, y2 string) string {
	return y1 + "---\n" + y2
}

// ExecuteTemplatedYAMLs takes a template YAML and applies the given template values to it.
func ExecuteTemplatedYAMLs(yamls []*YAMLFile, tmplValues *YAMLTmplArguments) ([]*YAMLFile, error) {
	// Execute the template on each of the YAMLs.
	executedYAMLs := make([]*YAMLFile, len(yamls))
	for i, y := range yamls {
		yamlFile := &YAMLFile{
			Name: y.Name,
		}

		if tmplValues == nil {
			yamlFile.YAML = y.YAML
		} else {
			executedYAML, err := executeTemplate(tmplValues, y.YAML)
			if err != nil {
				return nil, err
			}
			yamlFile.YAML = executedYAML
		}
		executedYAMLs[i] = yamlFile
	}

	return executedYAMLs, nil
}

func required(str string, value string) (string, error) {
	if value != "" {
		return value, nil
	}
	return "", errors.New("Value is required")
}

func executeTemplate(tmplValues *YAMLTmplArguments, tmplStr string) (string, error) {
	funcMap := template.FuncMap{
		"required": required,
	}

	tmpl, err := template.New("yaml").Funcs(funcMap).Parse(tmplStr)
	if err != nil {
		return "", err
	}
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, tmplValues)
	if err != nil {
		return "", err
	}

	return buf.String(), nil
}

// ExtractYAMLs writes the generated YAMLs to a tar at the given path in the given format.
func ExtractYAMLs(yamls []*YAMLFile, extractPath string, yamlDir string, format ExtractYAMLFormat) error {
	writeYAML := func(w *tar.Writer, name string, contents string) error {
		if err := w.WriteHeader(&tar.Header{Name: name, Size: int64(len(contents)), Mode: 511}); err != nil {
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
		break
	case SingleFileExtractYAMLFormat:
		// Combine all YAMLs into a single file.
		combinedYAML := ""
		for _, y := range yamls {
			combinedYAML = concatYAMLs(combinedYAML, y.YAML)
		}
		err = writeYAML(w, fmt.Sprintf("./%s/manifest.yaml", yamlDir), combinedYAML)
		if err != nil {
			return err
		}
		break
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
