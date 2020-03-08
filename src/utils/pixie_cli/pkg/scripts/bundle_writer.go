package scripts

import (
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"path"
	"path/filepath"
	"strings"

	"github.com/bmatcuk/doublestar"
	"gopkg.in/yaml.v2"
)

// BundleWriter creates script bundle files.
type BundleWriter struct {
	basePaths   []string
	searchPaths []string
}

type docSpec struct {
	Short string `yaml:"short"`
	Long  string `yaml:"long"`
}

// fileExists checks if a file exists and is not a directory before we
// try using it to prevent further errors.
func fileExists(filename string) bool {
	info, err := os.Stat(filename)
	if os.IsNotExist(err) {
		return false
	}
	return !info.IsDir()
}

// NewBundleWriter created a new BundleWriter.
func NewBundleWriter(searchPaths []string, basePaths []string) *BundleWriter {
	return &BundleWriter{
		basePaths:   basePaths,
		searchPaths: searchPaths,
	}
}

func (b BundleWriter) parseBundleScripts(basePath string) (*pixieScript, error) {
	pxlFiles, err := doublestar.Glob(path.Join(basePath, "*.pxl"))
	if err != nil {
		return nil, err
	}

	if len(pxlFiles) != 1 {
		return nil, fmt.Errorf("expected exactly one pxl script, found %d",
			len(pxlFiles))
	}

	ps := &pixieScript{}
	data, err := ioutil.ReadFile(pxlFiles[0])
	if err != nil {
		return nil, err
	}
	ps.Pxl = string(data)

	visFile := path.Join(basePath, "vis.json")
	placementFile := path.Join(basePath, "placement.json")
	docFile := path.Join(basePath, "doc.yaml")

	visFileExists := fileExists(visFile)
	placementFileExists := fileExists(placementFile)
	if visFileExists != placementFileExists {
		if placementFileExists {
			return nil, fmt.Errorf("if placement.json exists then vis.json needs to as well")
		}
		return nil, fmt.Errorf("if vis.json exists then placement.json needs to as well")
	}

	if visFileExists {
		data, err := ioutil.ReadFile(visFile)
		if err != nil {
			return nil, err
		}
		ps.Vis = string(data)

		data, err = ioutil.ReadFile(placementFile)
		if err != nil {
			return nil, err
		}
		ps.Placement = string(data)
	}

	f, err := os.Open(docFile)
	if err != nil {
		return nil, err
	}
	defer f.Close()

	var doc docSpec
	err = yaml.NewDecoder(f).Decode(&doc)
	if err != nil {
		return nil, err
	}

	ps.ShortDoc = doc.Short
	ps.LongDoc = doc.Long
	return ps, nil
}

// Writer writes the bundle file to the specified output.
func (b *BundleWriter) Write(outFile string) error {
	bundle := &bundle{
		Scripts: make(map[string]*pixieScript),
	}
	for _, sp := range b.searchPaths {
		absPath, _ := filepath.Abs(sp)
		for _, bp := range b.basePaths {
			matches, err := doublestar.Glob(path.Join(absPath, bp, "**/*.pxl"))
			if err != nil {
				return err
			}
			for _, m := range matches {
				absMatch, _ := filepath.Abs(m)
				absDir := filepath.Dir(absMatch)
				scriptName := strings.TrimPrefix(absDir, absPath+"/")
				ps, err := b.parseBundleScripts(absDir)
				if err != nil {
					return err
				}

				if _, has := bundle.Scripts[scriptName]; has {
					return fmt.Errorf("script %s already exists", scriptName)
				}
				bundle.Scripts[scriptName] = ps
			}
		}
	}

	var f io.Writer
	if outFile == "-" {
		f = os.Stdout
	} else {
		o, err := os.Create(outFile)
		if err != nil {
			return err
		}
		defer o.Close()
		f = o
	}

	return json.NewEncoder(f).Encode(bundle)
}
