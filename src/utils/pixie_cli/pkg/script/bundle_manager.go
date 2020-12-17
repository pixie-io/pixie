package script

import (
	"encoding/json"
	"errors"
	"io"
	"net/http"
	"net/url"
	"os"
	"sort"
	"sync"

	log "github.com/sirupsen/logrus"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/auth"
	cliLog "pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/utils"
)

// BundleManager reads a script bundle.
type BundleManager struct {
	scripts map[string]*pixieScript
}

func pixieScriptToExecutableScript(scriptName string, script *pixieScript) (*ExecutableScript, error) {
	vs, err := ParseVisSpec(script.Vis)
	if err != nil {
		return nil, err
	}
	return &ExecutableScript{
		ScriptName:   scriptName,
		ShortDoc:     script.ShortDoc,
		LongDoc:      script.LongDoc,
		Vis:          vs,
		ScriptString: script.Pxl,
		OrgName:      script.OrgName,
		Hidden:       script.Hidden,
	}, nil
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

// NewBundleManagerWithOrgName reads the json bundle and initializes the bundle reader for a specific org.
func NewBundleManagerWithOrgName(bundleFiles []string, orgName string) (*BundleManager, error) {
	var wg sync.WaitGroup
	wg.Add(len(bundleFiles))
	bundles := make([]*bundle, len(bundleFiles))

	readBundle := func(bundleFile string, index int) {
		defer wg.Done()
		var r io.Reader
		if isValidURL(bundleFile) {
			resp, err := http.Get(bundleFile)
			if err != nil {
				cliLog.WithError(err).Error("Error checking bundle file URL")
				return
			}
			defer resp.Body.Close()
			r = resp.Body
		} else {
			f, err := os.Open(bundleFile)
			if err != nil {
				cliLog.WithError(err).Error("Error reading bundle file")
				return
			}
			defer f.Close()
			r = f
		}

		var b bundle
		err := json.NewDecoder(r).Decode(&b)
		if err != nil {
			cliLog.WithError(err).Error("Error decoding bundle file")
			return
		}

		bundles[index] = &b
	}

	for i, bundle := range bundleFiles {
		go readBundle(bundle, i)
	}
	wg.Wait()

	filtered := make(map[string]*pixieScript, 0)
	// Filter scripts by org.
	for _, b := range bundles {
		if b != nil {
			for k, script := range b.Scripts {
				if len(script.OrgName) == 0 || script.OrgName == orgName || orgName == "" {
					filtered[k] = script
				}
			}
		}
	}

	return &BundleManager{
		scripts: filtered,
	}, nil
}

// NewBundleManager reads the json bundle and initializes the bundle reader.
func NewBundleManager(bundleFiles []string) (*BundleManager, error) {
	// TODO(zasgar): Refactor user login state, etc.
	authInfo, err := auth.MustLoadDefaultCredentials()
	if err != nil {
		// TODO(nserrino): Refactor logic to return error rather than log.Fatal,
		// which sends an event to Sentry.
		log.WithError(err).Fatal("Must be logged in")
	}

	return NewBundleManagerWithOrgName(bundleFiles, authInfo.OrgName)
}

// GetScriptMetadata returns metadata about available scripts.
func (b BundleManager) GetScripts() []*ExecutableScript {
	s := make([]*ExecutableScript, 0)
	i := 0
	for k, val := range b.scripts {
		pixieScript, err := pixieScriptToExecutableScript(k, val)
		if err != nil {
			cliLog.WithError(err).Error("Failed to parse script, skipping...")
			continue
		}
		s = append(s, pixieScript)
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
	return pixieScriptToExecutableScript(scriptName, script)
}

// MustGetScript is GetScript with fatal on error.
func (b BundleManager) MustGetScript(scriptName string) *ExecutableScript {
	es, err := b.GetScript(scriptName)
	if err != nil {
		// TODO(nserrino): Refactor to return an error rather than log.Fatal,
		// or fatal without log.Fatal, rather than this approach which sends an
		// unnecessary error event to sentry.
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
