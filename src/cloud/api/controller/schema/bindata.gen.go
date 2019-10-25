// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// schema.graphql
package schema

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

var _schemaGraphql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x93\x4f\x8f\xea\x36\x14\xc5\xf7\xf9\x14\xa7\x62\xd1\x56\x2a\x2c\xaa\xf6\x2d\xb2\x8b\x02\xed\x44\x9d\xc9\xbc\x0e\xd0\xd7\xd7\x0d\x32\xf1\x85\x58\x24\x76\xc6\xbe\x9e\x29\xaa\xde\x77\xaf\xec\x24\x10\xd0\xac\xc0\x37\xbe\xc7\xbf\xfb\xe7\xcc\xb0\xa9\x95\xc3\x41\x35\x04\x49\xae\xb2\x6a\x4f\x0e\x5c\x13\x5c\x55\x53\x2b\x70\xb0\xa6\x8d\xe7\xec\x73\x01\x47\xf6\x4d\x55\xb4\x48\x66\xc9\x0c\x05\x7f\xef\xa0\x0d\x43\x49\x12\xcd\x4f\xd8\x7b\xc6\x3b\x41\x13\x49\xb0\x41\x2b\xb4\x17\x4d\x73\xc6\x91\x34\x59\xc1\x04\x3e\x77\xe4\x70\x30\x36\xea\x6d\xce\x1d\xad\x2b\xab\x3a\xc6\xb6\x48\x66\x78\xaf\x49\x83\x2f\x30\xca\xc1\x77\x52\x30\xc9\x45\x8f\x58\x09\x8d\x3d\x41\x1a\x4d\xd8\x9f\x61\xbd\xd6\x4a\x1f\xd3\x64\x06\x1c\xad\xe8\xea\xd7\x66\xde\x23\xcf\xe3\x3b\xbd\xf2\xf8\xf6\x9c\xdd\x50\xd0\x62\xb8\x8c\xf9\xdc\x78\xee\x3c\x8f\x71\xb9\x60\x17\x31\x54\x55\xe3\x5d\x35\xcd\x04\xbc\x26\x0c\x97\x83\x76\x0f\xc8\xb5\xe0\xfe\xde\x9e\xd0\xa9\xea\x44\x12\xbe\x0b\x68\xe1\xfa\xb6\x58\x24\x43\x6f\x27\xfa\x31\xd3\xc1\xd5\xc6\x37\x12\xf4\xaf\x72\x0c\xa5\xfb\x76\x8b\x96\x20\x95\xa5\x8a\x8d\x3d\x43\x4c\x87\x70\x61\x0e\xe9\x8b\x24\x19\x46\xf3\x5f\x02\xbc\x7a\xb2\xe7\x14\x7f\x86\x9f\x04\x68\x3d\x0b\x56\x46\xa7\x78\x1a\xfe\x25\xdf\x92\x24\x42\x6f\x1d\xd9\x42\x1f\x4c\x4c\x53\x32\x45\xb1\xfc\x2e\x01\xb4\x68\x29\xc5\x9a\xad\xd2\xc7\x70\xa6\x56\xa8\x66\x1a\xe8\x54\xc5\xde\x4e\xee\x7c\x4b\x12\xd2\xbe\x45\x66\x59\x1d\x44\xc5\x61\x90\x51\x14\xc8\x36\xbb\x6d\xf9\x47\xf9\xfc\xa5\x1c\x8f\x8f\x45\xb9\xfd\x7b\x97\x3d\x2d\x3f\xfd\x32\x86\x96\xd9\xcb\x97\xa2\xbc\x8d\xe5\xcf\xe5\x26\x2b\xca\xd5\xcb\x6e\xbd\xda\xec\xbe\x66\x4f\x8f\xeb\x8f\x3f\x4d\xf5\xc6\xca\x62\xf1\x91\xc0\x3b\xb2\xe9\xa5\xd2\x40\x5f\x35\xde\x71\x08\xe6\xfd\x9f\xbb\x78\x6e\xb4\xa6\xaa\xef\x58\x7e\x1f\xba\xde\x55\x63\xad\x3f\x88\x49\xd1\xe9\x4d\x0b\x7e\x4c\x91\x3f\x16\x63\xe4\xda\xa6\x41\x76\xcd\x82\xbd\x8b\x94\x61\x2f\x82\xdb\x0e\xc2\x37\x0c\xc7\x61\xc5\xd4\x21\x58\xa9\x56\xfa\x18\x36\xff\xa4\xcd\xbb\x5e\x24\xc0\x5f\xff\xec\xd6\xd3\x9e\xf6\xa9\x43\x8a\x43\x4d\xa2\xe1\xfa\x1c\xb2\x6b\x12\x96\xf7\x24\xd8\x41\x58\x82\xa5\x8a\xd4\x1b\x49\x18\x0d\x4b\x47\xdf\x08\x0b\xa5\x99\xec\x9b\x68\x1c\x84\x96\x61\xbd\xa2\xe0\xd0\x89\x20\x67\xc9\x75\x46\xcb\x00\xc1\x06\x96\x5e\x3d\x39\x76\x57\x8e\x87\x55\xf6\xb8\x79\xf8\x7a\xc7\xd1\x7b\xc5\x84\x14\xa9\x5c\xd5\xb7\x8f\x64\x80\x0a\x2b\xfc\xfb\xcb\xe7\x1c\xd5\xa5\xa9\xd8\x5b\x12\x27\xb7\x88\x02\xb5\xe9\x62\x1d\xc1\x4b\x51\x35\x24\x8c\x40\x51\xb7\x32\x2d\x61\x2f\xaa\x13\x8c\x6e\x94\xa6\x88\x6e\xc9\xf9\x96\x82\x73\x06\xa2\x9e\xe4\x0a\xba\x2c\xd6\xf9\x73\x59\xae\xf2\xcd\x6a\x79\x59\x93\xc9\x06\xdc\x79\xc0\xc5\xd9\xa4\xb7\xa3\x0a\x1f\x1a\xe1\xf8\x61\x6c\xec\x93\x4b\xf1\x5b\x63\x44\x3f\xdb\xa9\xe4\xed\xca\xf4\xe2\x5d\x26\xa5\x25\xe7\xa6\x56\x62\x73\x22\x7d\x63\xa4\xa8\x32\x5a\x35\x26\xe6\x96\x04\x53\xfe\xc1\xda\x5e\x5f\xbd\xae\xd9\xe0\x3b\x6f\x6f\x2c\x0b\xb8\x5a\xfc\xfc\xeb\xa7\xe9\x53\xff\x07\x00\x00\xff\xff\x3e\xc8\x80\x65\xe9\x05\x00\x00")

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

	info := bindataFileInfo{name: "schema.graphql", size: 1513, mode: os.FileMode(436), modTime: time.Unix(1572034832, 0)}
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
