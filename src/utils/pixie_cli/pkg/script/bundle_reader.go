package script

import (
	"encoding/json"
	"io"
	"net/http"
	"net/url"
	"os"

	log "github.com/sirupsen/logrus"
)

// BundleReader reads a script bundle.
type BundleReader struct {
	scripts map[string]*pixieScript
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
func (b BundleReader) GetScriptMetadata() []Metadata {
	s := make([]Metadata, len(b.scripts))
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
func (b BundleReader) GetScript(scriptName string) (*ExecutableScript, error) {
	script, ok := b.scripts[scriptName]
	if !ok {
		return nil, ErrScriptNotFound
	}
	return &ExecutableScript{
		metadata: Metadata{
			ScriptName: scriptName,
			ShortDoc:   script.ShortDoc,
			LongDoc:    script.LongDoc,
			HasVis:     script.Pxl != "",
		},
		scriptString: script.Pxl,
	}, nil
}

// MustGetScript is GetScript with fatal on error.
func (b BundleReader) MustGetScript(scriptName string) *ExecutableScript {
	es, err := b.GetScript(scriptName)
	if err != nil {
		log.WithError(err).Fatal("Failed to get script")
	}
	return es
}
