package scripts

import (
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
)

// BundleReader reads a script bundle.
type BundleReader struct {
	scripts map[string]*pixieScript
}

// ScriptMetadata has metadata information about a specific script.
type ScriptMetadata struct {
	ScriptName string
	ShortDoc   string
	LongDoc    string
}

func isValidURL(toTest string) bool {
	_, err := url.ParseRequestURI(toTest)
	if err != nil {
		return false
	}

	u, err := url.Parse(toTest)
	if err != nil || u.Scheme == "" || u.Host == "" {
		return false
	}

	return true
}

// NewBundleReader reads the json bundle and initializes the bundle reader.
func NewBundleReader(bundleFile string) (*BundleReader, error) {
	var r io.Reader
	if isValidURL(bundleFile) {
		resp, err := http.Get(bundleFile)
		if err != nil {
			return nil, err
		}
		defer resp.Body.Close()
		r = resp.Body
	} else {
		f, err := os.Open(bundleFile)
		if err != nil {
			return nil, err
		}
		defer f.Close()
		r = f
	}

	var b bundle
	err := json.NewDecoder(r).Decode(&b)
	if err != nil {
		return nil, err
	}
	return &BundleReader{
		scripts: b.Scripts,
	}, nil
}

// GetScriptMetadata returns metadata about available scripts.
func (b BundleReader) GetScriptMetadata() []ScriptMetadata {
	s := make([]ScriptMetadata, len(b.scripts))
	i := 0
	for k, val := range b.scripts {
		s[i].ScriptName = k
		s[i].ShortDoc = val.ShortDoc
		s[i].LongDoc = val.LongDoc
		i++
	}
	return s
}

// GetScript returns the script by name.
func (b BundleReader) GetScript(scriptName string) (string, bool, error) {
	script, ok := b.scripts[scriptName]
	if !ok {
		return "", false, fmt.Errorf("script '%s' not found", scriptName)
	}
	return script.Pxl, script.Vis != "", nil
}
