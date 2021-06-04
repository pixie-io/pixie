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

var _schemaGraphql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x64\x91\xc1\x8e\xd4\x30\x0c\x86\xef\x79\x8a\x7f\xd4\x03\x20\xd1\x3e\x40\x6f\x7b\x41\xea\x01\x04\xda\xdd\x13\xe2\x90\x26\x6e\x63\x91\x26\xdd\xc4\x9d\xa1\x42\xbc\x3b\x6a\xda\x8e\x06\x71\x6a\x6d\xff\xfe\xfd\xc5\xae\xf0\xe2\x38\x63\x60\x4f\xb0\x94\x4d\xe2\x9e\x32\xc4\x11\xb2\x71\x34\x69\x0c\x29\x4e\x25\x7e\xfa\xda\x21\x53\xba\xb2\xa1\x46\x55\xaa\x42\x27\xef\x32\x42\x14\xb0\x25\xed\x3f\xa2\x5f\x04\x37\x42\x20\xb2\x90\x88\x49\x87\x45\x7b\xbf\x62\xa4\x40\x49\x0b\x41\xd6\x99\x32\x86\x98\x8a\xdf\xcb\x3a\xd3\xb3\x49\x3c\x0b\x5e\x3b\x55\xe1\xe6\x28\x40\xee\x30\x9c\xb1\xcc\x56\x0b\xd9\x66\x47\x34\x3a\xa0\x27\xd8\x18\x08\xfd\x8a\xb4\x84\xc0\x61\x6c\x55\x05\x8c\x49\xcf\xee\xcd\xd7\x3b\x72\x5d\xe6\xec\xce\xe7\xec\x5a\xf2\xf1\xa0\xe6\x10\xa3\xae\xe3\x22\xf3\x22\x67\xde\x36\x92\x0b\x06\x1b\x87\x1b\x7b\xff\x00\xee\x08\x87\x78\xf3\xde\x01\xc5\x69\xd9\x75\x3d\x61\x66\xf3\x93\x2c\x96\x79\x43\xdb\xe4\xaf\x5d\xa3\x8e\xdd\x3e\xf8\x97\xce\x8c\xec\xe2\xe2\x2d\xe8\x17\x67\x01\x87\x7d\xdd\x7a\x22\x58\x4e\x64\x24\xa6\x15\xfa\xf1\x08\x77\xe6\xad\xbd\x51\xea\x38\xcd\x6f\x05\xbc\x2d\x94\xd6\x16\xdf\xb6\x8f\xfa\xa3\x54\xe1\x2b\x51\x29\xeb\x24\x3c\x68\x23\xf9\xfd\xf9\xf7\x45\x4f\xd4\xe2\x59\x12\x87\xf1\xf2\xa1\xc5\xd3\xa9\xe8\xc2\x10\x2f\x77\x8b\x7f\xd2\xc5\x8a\x85\xa6\xdc\xe2\xfb\x59\xb9\xfc\xf8\x5f\x5d\x84\x57\x4a\x99\x63\xb8\x0f\x51\x80\x71\x3a\x8c\xe4\xe3\xf8\x98\x14\x9e\x28\x8b\x9e\xe6\xcf\xb9\xc5\x27\x1f\xb5\x6c\x86\x7f\x03\x00\x00\xff\xff\xc0\x17\xd2\xb7\x91\x02\x00\x00")

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

	info := bindataFileInfo{name: "schema.graphql", size: 657, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
