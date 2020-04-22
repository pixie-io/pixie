package script

import (
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"net/url"
	"os"
	"sort"

	log "github.com/sirupsen/logrus"
)

// BundleManager reads a script bundle.
type BundleManager struct {
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

// NewBundleManager reads the json bundle and initializes the bundle reader.
func NewBundleManager(bundleFile string) (*BundleManager, error) {
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
	return &BundleManager{
		scripts: b.Scripts,
	}, nil
}

// GetScriptMetadata returns metadata about available scripts.
func (b BundleManager) GetScripts() []*ExecutableScript {
	s := make([]*ExecutableScript, len(b.scripts))
	i := 0
	for k, val := range b.scripts {
		s[i].ScriptName = k
		s[i].ScriptString = val.Pxl
		s[i].ShortDoc = val.ShortDoc
		s[i].LongDoc = val.LongDoc
		i++
	}
	return s
}

// GetOrderedScriptMetadata returns metadata about available scripts ordered by the name of the script.
func (b BundleManager) GetOrderedScripts() []*ExecutableScript {
	s := b.GetScripts()
	sort.Slice(s, func(i, j int) bool {
		return s[i].ScriptName < s[j].ScriptName
	})
	return s
}

// GetScript returns the script by name.
func (b BundleManager) GetScript(scriptName string) (*ExecutableScript, error) {
	script, ok := b.scripts[scriptName]
	if !ok {
		return nil, ErrScriptNotFound
	}
	return &ExecutableScript{
		ScriptName:   scriptName,
		ShortDoc:     script.ShortDoc,
		LongDoc:      script.LongDoc,
		HasVis:       script.Vis != "",
		ScriptString: script.Pxl,
	}, nil
}

// MustGetScript is GetScript with fatal on error.
func (b BundleManager) MustGetScript(scriptName string) *ExecutableScript {
	es, err := b.GetScript(scriptName)
	if err != nil {
		log.WithError(err).Fatal("Failed to get script")
	}
	return es
}

// AddScript adds the specified script to the bundle manager.
func (b *BundleManager) AddScript(script *ExecutableScript) error {
	n := script.ScriptName

	_, has := b.scripts[n]
	if has {
		return errors.New("script with same name already exists")
	}
	p := &pixieScript{
		Pxl:      script.ScriptString,
		ShortDoc: script.ShortDoc,
		LongDoc:  script.LongDoc,
	}
	b.scripts[n] = p
	return nil
}
