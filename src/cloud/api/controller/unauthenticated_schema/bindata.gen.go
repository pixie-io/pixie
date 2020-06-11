// Code generated for package unauthenticatedschema by go-bindata DO NOT EDIT. (@generated)
// sources:
// schema.graphql
package unauthenticatedschema

import (
	"bytes"
	"compress/gzip"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"path/filepath"
	"strings"
	"time"
)

func bindataRead(data []byte, name string) ([]byte, error) {
	gz, err := gzip.NewReader(bytes.NewBuffer(data))
	if err != nil {
		return nil, fmt.Errorf("Read %q: %v", name, err)
	}

	var buf bytes.Buffer
	_, err = io.Copy(&buf, gz)
	clErr := gz.Close()

	if err != nil {
		return nil, fmt.Errorf("Read %q: %v", name, err)
	}
	if clErr != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

type asset struct {
	bytes []byte
	info  os.FileInfo
}

type bindataFileInfo struct {
	name    string
	size    int64
	mode    os.FileMode
	modTime time.Time
}

// Name return file name
func (fi bindataFileInfo) Name() string {
	return fi.name
}

// Size return file size
func (fi bindataFileInfo) Size() int64 {
	return fi.size
}

// Mode return file mode
func (fi bindataFileInfo) Mode() os.FileMode {
	return fi.mode
}

// Mode return file modify time
func (fi bindataFileInfo) ModTime() time.Time {
	return fi.modTime
}

// IsDir return file whether a directory
func (fi bindataFileInfo) IsDir() bool {
	return fi.mode&os.ModeDir != 0
}

// Sys return file is sys mode
func (fi bindataFileInfo) Sys() interface{} {
	return nil
}

var _schemaGraphql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x91\xc1\x6f\xd3\x30\x18\xc5\xef\xfe\x2b\xde\xd4\x03\x20\xd1\x9c\x10\x87\xdc\x22\x36\xa4\x88\x35\x83\xb5\xd5\x40\x08\x55\x6e\xf2\x35\xb1\x70\xec\xcc\xfe\xbc\x12\xa1\xfd\xef\x28\x4e\x52\x65\x62\xa7\xc4\xcf\xcf\xef\xfb\xf9\x79\x85\x5d\xa3\x3c\x4e\x4a\x13\x2a\xf2\xa5\x53\x47\xf2\xe0\x86\xe0\xcb\x86\x5a\x89\x93\xb3\x6d\x5c\x67\x5f\x73\x78\x72\x4f\xaa\xa4\x44\xac\xc4\x0a\x39\xbf\xf1\x30\x96\xa1\x2a\x92\xfa\x3d\x8e\x81\x71\x26\x18\xa2\x0a\x6c\xd1\x4a\x13\xa4\xd6\x3d\x6a\x32\xe4\x24\x13\xb8\xef\xc8\xe3\x64\x5d\xcc\xdb\xf5\x1d\x6d\x4b\xa7\x3a\xc6\x3e\x17\x2b\x9c\x1b\x32\xe0\x0b\x8c\xf2\x08\x5d\x25\x99\xaa\x64\x44\x2c\xa5\xc1\x91\x50\x59\x43\x38\xf6\x70\xc1\x18\x65\xea\x54\xac\x80\xda\xc9\xae\x79\xd4\xeb\x11\x79\x1d\xe7\x8c\xc9\xf3\xec\x35\xfb\xe9\x42\xc9\x64\xc6\x7a\x6d\x03\x77\x81\x67\xbd\x4a\xd8\x47\x0c\x55\x36\x38\x2b\xad\x17\xe0\x0d\x61\x32\x0f\xd9\x23\x20\x37\x92\x47\xdf\x91\xd0\xa9\xf2\x37\x55\x08\xdd\x80\x36\xd8\xf7\x79\x22\xa6\x6e\x17\xf9\xf1\xa4\x87\x6f\x6c\xd0\x15\xe8\x8f\xf2\x0c\x65\xc6\xba\x65\x4b\xa8\x94\xa3\x92\xad\xeb\x21\x97\x8f\x70\x61\x1e\x8e\x27\x42\x4c\x4f\xf3\x57\x00\x8f\x81\x5c\x9f\xe2\xdb\xf0\x11\xcf\x42\x90\x09\x2d\x32\xc7\xea\x24\x4b\x1e\x2a\x8e\x2e\x20\xdb\x1d\xf6\xc5\x97\xe2\xee\xa1\x98\x97\xb7\x79\xb1\xff\x7e\xc8\x36\xd7\x1f\x3f\xcc\xd2\x75\x76\xff\x90\x17\x2f\xb5\x4f\x77\xc5\x2e\xcb\x8b\x9b\xfb\xc3\xf6\x66\x77\xf8\x91\x6d\x6e\xb7\xaf\x6f\x2d\xf3\x9e\x85\x88\x45\x45\xac\x48\x20\x27\x24\xff\x76\xfe\x2b\x64\x4b\x29\xb6\xec\x94\xa9\xdf\xa5\x17\x66\x9f\x9b\x93\xbd\xba\x24\xbc\x90\x63\x92\x62\x6a\x7d\x8a\x9f\xf3\xce\xaf\xff\xbc\xd1\xf6\x44\xce\x2b\x6b\xe6\x09\x57\x02\x28\x1b\x69\x6a\xd2\xb6\x5e\x8a\xac\x5a\xf2\x2c\xdb\x6e\xe3\x53\x7c\xd6\x56\xf2\x30\xfc\x5f\x00\x00\x00\xff\xff\x30\x39\xfb\x92\x17\x03\x00\x00")

func schemaGraphqlBytes() ([]byte, error) {
	return bindataRead(
		_schemaGraphql,
		"schema.graphql",
	)
}

func schemaGraphql() (*asset, error) {
	bytes, err := schemaGraphqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "schema.graphql", size: 791, mode: os.FileMode(436), modTime: time.Unix(1591900751, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

// Asset loads and returns the asset for the given name.
// It returns an error if the asset could not be found or
// could not be loaded.
func Asset(name string) ([]byte, error) {
	cannonicalName := strings.Replace(name, "\\", "/", -1)
	if f, ok := _bindata[cannonicalName]; ok {
		a, err := f()
		if err != nil {
			return nil, fmt.Errorf("Asset %s can't read by error: %v", name, err)
		}
		return a.bytes, nil
	}
	return nil, fmt.Errorf("Asset %s not found", name)
}

// MustAsset is like Asset but panics when Asset would return an error.
// It simplifies safe initialization of global variables.
func MustAsset(name string) []byte {
	a, err := Asset(name)
	if err != nil {
		panic("asset: Asset(" + name + "): " + err.Error())
	}

	return a
}

// AssetInfo loads and returns the asset info for the given name.
// It returns an error if the asset could not be found or
// could not be loaded.
func AssetInfo(name string) (os.FileInfo, error) {
	cannonicalName := strings.Replace(name, "\\", "/", -1)
	if f, ok := _bindata[cannonicalName]; ok {
		a, err := f()
		if err != nil {
			return nil, fmt.Errorf("AssetInfo %s can't read by error: %v", name, err)
		}
		return a.info, nil
	}
	return nil, fmt.Errorf("AssetInfo %s not found", name)
}

// AssetNames returns the names of the assets.
func AssetNames() []string {
	names := make([]string, 0, len(_bindata))
	for name := range _bindata {
		names = append(names, name)
	}
	return names
}

// _bindata is a table, holding each asset generator, mapped to its name.
var _bindata = map[string]func() (*asset, error){
	"schema.graphql": schemaGraphql,
}

// AssetDir returns the file names below a certain
// directory embedded in the file by go-bindata.
// For example if you run go-bindata on data/... and data contains the
// following hierarchy:
//     data/
//       foo.txt
//       img/
//         a.png
//         b.png
// then AssetDir("data") would return []string{"foo.txt", "img"}
// AssetDir("data/img") would return []string{"a.png", "b.png"}
// AssetDir("foo.txt") and AssetDir("notexist") would return an error
// AssetDir("") will return []string{"data"}.
func AssetDir(name string) ([]string, error) {
	node := _bintree
	if len(name) != 0 {
		cannonicalName := strings.Replace(name, "\\", "/", -1)
		pathList := strings.Split(cannonicalName, "/")
		for _, p := range pathList {
			node = node.Children[p]
			if node == nil {
				return nil, fmt.Errorf("Asset %s not found", name)
			}
		}
	}
	if node.Func != nil {
		return nil, fmt.Errorf("Asset %s not found", name)
	}
	rv := make([]string, 0, len(node.Children))
	for childName := range node.Children {
		rv = append(rv, childName)
	}
	return rv, nil
}

type bintree struct {
	Func     func() (*asset, error)
	Children map[string]*bintree
}

var _bintree = &bintree{nil, map[string]*bintree{
	"schema.graphql": &bintree{schemaGraphql, map[string]*bintree{}},
}}

// RestoreAsset restores an asset under the given directory
func RestoreAsset(dir, name string) error {
	data, err := Asset(name)
	if err != nil {
		return err
	}
	info, err := AssetInfo(name)
	if err != nil {
		return err
	}
	err = os.MkdirAll(_filePath(dir, filepath.Dir(name)), os.FileMode(0755))
	if err != nil {
		return err
	}
	err = ioutil.WriteFile(_filePath(dir, name), data, info.Mode())
	if err != nil {
		return err
	}
	err = os.Chtimes(_filePath(dir, name), info.ModTime(), info.ModTime())
	if err != nil {
		return err
	}
	return nil
}

// RestoreAssets restores an asset under the given directory recursively
func RestoreAssets(dir, name string) error {
	children, err := AssetDir(name)
	// File
	if err != nil {
		return RestoreAsset(dir, name)
	}
	// Dir
	for _, child := range children {
		err = RestoreAssets(dir, filepath.Join(name, child))
		if err != nil {
			return err
		}
	}
	return nil
}

func _filePath(dir, name string) string {
	cannonicalName := strings.Replace(name, "\\", "/", -1)
	return filepath.Join(append([]string{dir}, strings.Split(cannonicalName, "/")...)...)
}
