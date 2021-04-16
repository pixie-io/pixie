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

var _schemaGraphql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x91\xc1\x6f\x9b\x30\x18\xc5\xef\xfe\x2b\x5e\x95\xc3\x36\x69\xe1\x34\xed\xc0\x0d\xad\x99\x84\x96\xd0\xae\x21\xea\xa6\x69\x8a\x1c\xf8\x02\xd6\xc0\xa6\xf6\xe7\x66\x68\xea\xff\x3e\x61\x20\x4a\xd4\x9e\xc0\xcf\xcf\xef\xfb\xf9\x79\x81\xbc\x56\x0e\x47\xd5\x10\x4a\x72\x85\x55\x07\x72\xe0\x9a\xe0\x8a\x9a\x5a\x89\xa3\x35\x6d\x58\x27\xf7\x29\x1c\xd9\x67\x55\x50\x24\x16\x62\x81\x94\xdf\x39\x68\xc3\x50\x25\xc9\xe6\x23\x0e\x9e\x71\x22\x68\xa2\x12\x6c\xd0\x4a\xed\x65\xd3\xf4\xa8\x48\x93\x95\x4c\xe0\xbe\x23\x87\xa3\xb1\x21\x2f\xef\x3b\xda\x16\x56\x75\x8c\x5d\x2a\x16\x38\xd5\xa4\xc1\x67\x18\xe5\xe0\xbb\x52\x32\x95\xd1\x88\x58\x48\x8d\x03\xa1\x34\x9a\x70\xe8\x61\xbd\xd6\x4a\x57\xb1\x58\x00\x95\x95\x5d\xfd\xd4\x2c\x47\xe4\x65\x98\x33\x26\xcf\xb3\x97\xec\xa6\x0b\x45\x93\x19\xcb\xa5\xf1\xdc\x79\x9e\xf5\x32\x62\x17\x30\x54\x51\xe3\xa4\x9a\xe6\x02\xbc\x26\x4c\xe6\x21\x7b\x04\xe4\x5a\xf2\xe8\x3b\x10\x3a\x55\xfc\xa1\x12\xbe\x1b\xd0\x06\xfb\x2e\x8d\xc4\xd4\xed\x45\x7e\x38\xe9\xe0\x6a\xe3\x9b\x12\xf4\x57\x39\x86\xd2\x63\xdd\xb2\x25\x94\xca\x52\xc1\xc6\xf6\x90\x97\x8f\x70\x66\x1e\x8e\x47\x42\x4c\x4f\xf3\x4f\x00\x4f\x9e\x6c\x1f\xe3\xfb\xf0\x11\x2f\x42\x90\xf6\x2d\x12\xcb\xea\x28\x0b\x1e\x2a\x0e\x2e\x20\xc9\xf7\xbb\xec\x5b\x76\xf7\x98\xcd\xcb\x75\x9a\xed\x7e\xec\x93\xcd\xed\xe7\x4f\xb3\x74\x9b\x3c\x3c\xa6\xd9\xb5\xf6\xe5\x2e\xcb\x93\x34\x5b\x3d\xec\xb7\xab\x7c\xff\x33\xd9\xac\xb7\x6f\x6f\xbd\x91\x77\x6d\xc8\x57\x9b\xfb\x75\x92\xaf\xa6\x90\x17\x21\x42\x99\x01\x3d\x50\xca\x09\xdb\xbd\x9f\xff\x32\xd9\x52\x8c\x2d\x5b\xa5\xab\x0f\xf1\xf9\x5e\x2e\xd5\x47\x73\x73\x4e\xb8\x92\x43\x92\x62\x6a\x5d\x8c\x5f\xf3\xce\xef\x57\xde\x60\x7b\x26\xeb\x94\xd1\xf3\x84\x1b\x01\x14\xb5\xd4\x15\x35\xa6\xba\x14\x59\xb5\xe4\x58\xb6\xdd\xc6\xc5\xf8\xda\x18\xc9\xc3\xf0\xff\x01\x00\x00\xff\xff\x08\xa0\xb1\xd4\x3b\x03\x00\x00")

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

	info := bindataFileInfo{name: "schema.graphql", size: 827, mode: os.FileMode(436), modTime: time.Unix(1617081765, 0)}
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
