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

var _schemaGraphql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x53\x4d\x8f\xdb\x36\x10\xbd\xeb\x57\x4c\xb0\x87\x36\x40\xed\x43\xd1\xe6\xa0\x9b\x21\xbb\x5d\xa1\xbb\xda\x74\x6d\x37\x4d\x8b\xc2\xa0\xc9\xb1\x45\x98\x22\xb5\x9c\xe1\x6e\x8d\x22\xff\xbd\x20\x25\xd9\x92\x93\x93\xad\xe1\xbc\x37\x6f\x3e\xde\x1d\x6c\x6a\x4d\x70\xd0\x06\x41\x21\x49\xaf\xf7\x48\xc0\x35\x02\xc9\x1a\x1b\x01\x07\xef\x9a\xf4\xbd\xf8\x58\x02\xa1\x7f\xd5\x12\xe7\xd9\x5d\x76\x07\x25\x7f\x47\x60\x1d\x83\x56\x28\xcc\x0f\xb0\x0f\x0c\x6f\x08\x16\x51\x01\x3b\x68\x84\x0d\xc2\x98\x33\x1c\xd1\xa2\x17\x8c\xc0\xe7\x16\x09\x0e\xce\x27\xbe\xcd\xb9\xc5\xb5\xf4\xba\x65\xd8\x96\xd9\x1d\xbc\xd5\x68\x81\x2f\x62\x34\x41\x68\x95\x60\x54\xf3\x4e\xa2\x14\x16\xf6\x08\xca\x59\x84\xfd\x19\x7c\xb0\x56\xdb\x63\x9e\xdd\x01\x1c\xbd\x68\xeb\x17\x33\xeb\x24\xcf\x52\x9d\x8e\x79\xa8\x3d\x63\xea\x1b\x9a\xf7\xc9\x30\x9b\xb9\xc0\x6d\xe0\x21\xae\xe6\x4c\x49\x86\x96\x35\xbc\x69\x63\x46\xc2\x6b\x84\x3e\x39\x72\x77\x02\xb9\x16\xdc\xe5\xed\x11\x5a\x2d\x4f\xa8\x20\xb4\x51\x5a\x4c\xdf\x96\xf3\xac\x9f\xed\x88\x3f\x21\x09\xa8\x76\xc1\x28\xc0\x7f\x35\x31\x68\xdb\x8d\x5b\x34\x08\x4a\x7b\x94\xec\xfc\x19\xc4\x78\x09\x17\xcd\x11\x3e\xcf\xb2\x7e\x35\xff\x65\x00\x2f\x01\xfd\x39\x87\xdf\xe3\x4f\x06\xd0\x04\x16\xac\x9d\xcd\xe1\xb1\xff\x97\x7d\xc9\xb2\x24\x7a\x4b\xe8\x4b\x7b\x70\x09\xa6\x55\x0e\xe5\xf2\x5d\x06\x60\x45\x83\x39\xac\xd9\x6b\x7b\x8c\xdf\xd8\x08\x6d\xc6\x81\x56\x4b\x0e\x7e\x94\xf3\x25\xcb\xd0\x86\x06\x16\x9e\xf5\x41\x48\x8e\x8b\x4c\xa4\x00\x8b\xcd\x6e\x5b\xfd\x56\x3d\x7d\xaa\x86\xcf\x87\xb2\xda\xfe\xb9\x5b\x3c\x2e\x3f\xfc\x34\x84\x96\x8b\xe7\x4f\x65\x35\x8d\x15\x4f\xd5\x66\x51\x56\xab\xe7\xdd\x7a\xb5\xd9\x7d\x5e\x3c\x3e\xac\xbf\xfd\x34\xe6\x1b\x3a\x4b\xcd\x27\x05\x81\xd0\xe7\x97\x4e\xa3\x7a\x69\x02\x71\x0c\x16\xdd\x9f\x9b\x78\xe1\xac\x45\xd9\x4d\xac\xb8\x0d\x5d\x73\xf5\xd0\xeb\xf7\x62\xd4\x74\x3e\x19\xc1\xfb\x1c\x8a\x87\x72\x88\x44\xdc\x90\x4b\x17\x54\x35\x1a\xf6\xfb\x2b\x9c\xba\x4a\xc3\x60\x7b\x21\x6b\x16\x1c\x28\xf5\x15\x2f\x29\xfa\xf3\x20\x82\x61\x20\x8e\x47\xa9\x0f\xd1\x7c\xb5\xb6\xc7\xe8\x95\x93\x75\x6f\x76\x9e\x01\xfc\xf1\xd7\x6e\x3d\xde\x42\x07\xed\x21\x04\x35\x0a\xc3\xf5\x39\xa2\x6b\x14\x9e\xf7\x28\x98\x40\x78\x04\x8f\x12\xf5\x2b\x2a\x70\x16\x3c\x1e\x83\x11\x1e\xb4\x65\xf4\xaf\xc2\x10\x08\xab\xe2\x41\x26\xc2\x7e\x76\x91\xce\x23\xb5\xce\xaa\x28\x82\x1d\x78\x7c\x09\x48\x4c\x57\x1d\xf7\xab\xc5\xc3\xe6\xfe\xf3\x8d\x8e\xce\x5d\x2e\x42\x94\x26\xd9\x0d\x1c\x55\x14\x15\x8f\xfe\xd7\xe7\x8f\x05\xc8\xcb\x1a\x60\xef\x51\x9c\x68\x9e\x08\x6a\xd7\xa6\x3e\xa2\xfb\x12\x6b\x04\x0c\x82\x12\xaf\x74\x0d\xc2\x5e\xc8\x13\x38\x6b\xb4\xc5\x24\xdd\x23\x85\x06\xa3\xd7\x7a\x45\x9d\x92\xab\xd0\x65\xb9\x2e\x9e\xaa\x6a\x55\x6c\x56\xcb\xcb\x61\x8d\x6e\xe6\xc6\x35\x94\x76\x93\x4f\x57\x15\x1f\x8c\x20\xbe\x1f\x06\xfb\x48\x39\xfc\x62\x9c\xe0\x77\xb7\x94\xd3\x23\xeb\xc8\xdb\x85\x52\x1e\x89\xc6\xe6\x63\x77\x42\x3b\xb1\x5e\x62\x19\xcc\x9d\x80\x85\x47\xc1\x58\x7c\xe3\xd0\xaf\x55\xaf\x87\xd9\x3b\x35\xf8\x89\xc9\x01\xa8\x16\x3f\xfe\xfc\xe1\xeb\x52\x93\x1b\xed\x84\x32\x36\x94\xc3\xdf\xc3\xcb\x3f\x5f\xe5\xa6\xb4\x57\xf4\x94\x7c\x75\xad\x22\x6b\x61\x8f\x68\xdc\x71\xd2\xa2\x6e\x90\x58\x34\xed\x64\x5a\xff\x07\x00\x00\xff\xff\xa8\x95\xeb\x7e\x98\x06\x00\x00")

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

	info := bindataFileInfo{name: "schema.graphql", size: 1688, mode: os.FileMode(436), modTime: time.Unix(1572395633, 0)}
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
