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
		return nil, fmt.Errorf("read %q: %v", name, err)
	}

	var buf bytes.Buffer
	_, err = io.Copy(&buf, gz)
	clErr := gz.Close()

	if err != nil {
		return nil, fmt.Errorf("read %q: %v", name, err)
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

// ModTime return file modify time
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

var _schemaGraphql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x7c\x94\xcd\x8e\xdb\x36\x10\xc7\xef\x7a\x8a\x09\x7c\x68\x02\xc4\x3e\x14\x6d\x0e\xba\xb9\xb2\xdb\x15\xba\xab\x4d\xd7\x76\xd2\xb4\x28\x0c\x5a\x1c\x4b\x84\x29\x52\xcb\x19\x7a\xeb\x16\xfb\xee\x05\x29\xc9\x96\x9d\xa0\x27\x49\xa3\x99\xe1\x6f\x3e\xfe\x9c\xc0\xba\x56\x04\x7b\xa5\x11\x24\x52\xe9\xd4\x0e\x09\xb8\x46\xa0\xb2\xc6\x46\xc0\xde\xd9\x26\x7e\xcf\x3f\xe6\x40\xe8\x8e\xaa\xc4\x59\x32\x49\x26\x90\xf3\x77\x04\xc6\x32\x28\x89\x42\xbf\x87\x9d\x67\x78\x41\x30\x88\x12\xd8\x42\x23\x8c\x17\x5a\x9f\xa0\x42\x83\x4e\x30\x02\x9f\x5a\x24\xd8\x5b\x17\xf3\xad\x4f\x2d\xae\x4a\xa7\x5a\x86\x4d\x9e\x4c\xe0\xa5\x46\x03\x7c\x86\x51\x04\xbe\x95\x82\x51\xce\x3a\xc4\x52\x18\xd8\x21\x48\x6b\x10\x76\x27\x70\xde\x18\x65\xaa\x34\x99\x00\x54\x4e\xb4\xf5\xb3\x9e\x76\xc8\xd3\x78\x4e\x97\x79\x38\x7b\xca\xd4\x17\x34\xeb\x9d\x61\x3a\xb5\x9e\x5b\xcf\x83\x5d\xce\x98\x22\x86\x2a\x6b\x78\x51\x5a\x8f\xc0\x6b\x84\xde\x39\xe4\xee\x00\xb9\x16\xdc\xf9\xed\x10\x5a\x55\x1e\x50\x82\x6f\x03\x5a\x70\xdf\xe4\xb3\xa4\xef\xed\x28\x7f\x8c\x24\xa0\xda\x7a\x2d\x01\xff\x56\xc4\xa0\x4c\xd7\x6e\xd1\x20\x48\xe5\xb0\x64\xeb\x4e\x20\xc6\x43\x38\x33\x87\xf0\x59\x92\xf4\xa3\xf9\x37\x01\x78\xf6\xe8\x4e\x29\xfc\x16\x1e\x09\x40\xe3\x59\xb0\xb2\x26\x85\x87\xfe\x2d\x79\x4d\x92\x08\xbd\x21\x74\xb9\xd9\xdb\x18\xa6\x64\x0a\xf9\xe2\x4d\x02\x60\x44\x83\x29\xac\xd8\x29\x53\x85\x6f\x6c\x84\xd2\x63\x43\xab\x4a\xf6\x6e\xe4\xf3\x9a\x24\x68\x7c\x03\x73\xc7\x6a\x2f\x4a\x0e\x83\x8c\x49\x01\xe6\xeb\xed\xa6\xf8\xb5\x78\xfc\x5c\x0c\x9f\xf7\x79\xb1\xf9\x7d\x3b\x7f\x58\x7c\xf8\x61\x30\x2d\xe6\x4f\x9f\xf3\xe2\xda\x96\x3d\x16\xeb\x79\x5e\x2c\x9f\xb6\xab\xe5\x7a\xfb\x65\xfe\x70\xbf\xfa\xf6\xaf\x71\xbe\xa1\xb2\x58\x7c\x24\xf0\x84\x2e\x3d\x57\x1a\xe8\x4b\xed\x89\x83\x31\xeb\x5e\x6e\xec\x99\x35\x06\xcb\xae\x63\xd9\xad\xe9\xe2\xab\x86\x5a\xdf\x8a\x51\xd1\xe9\x55\x0b\xde\xa5\x90\xdd\xe7\x83\x25\xc4\x0d\xbe\x74\x8e\x2a\x46\xcd\x7e\x77\x09\xa7\xee\xa4\xa1\xb1\x3d\xc8\x8a\x05\x7b\x8a\x75\x85\x4d\x0a\xfa\xdc\x0b\xaf\x19\x88\xc3\x52\xaa\x7d\x10\x5f\xad\x4c\x15\xb4\x72\x30\xf6\xc5\xcc\x12\x80\x4f\x7f\x6c\x57\xe3\x29\x74\xa1\x7d\x08\x41\x8d\x42\x73\x7d\x0a\xd1\x35\x0a\xc7\x3b\x14\x4c\x20\x1c\x82\xc3\x12\xd5\x11\x25\x58\x03\x0e\x2b\xaf\x85\x03\x65\x18\xdd\x51\x68\x02\x61\x64\x58\xc8\x98\xb0\xef\x5d\x48\xe7\x90\x5a\x6b\x64\x80\x60\x0b\x0e\x9f\x3d\x12\xd3\x85\xe3\x6e\x39\xbf\x5f\xdf\x7d\xb9\xe1\xe8\xd4\x65\x43\x88\x54\x54\x76\x0d\x47\x19\xa0\xc2\xd2\xff\xf2\xf4\x31\x83\xf2\x3c\x06\xd8\x39\x14\x07\x9a\xc5\x04\xb5\x6d\x63\x1d\x41\x7d\x31\x6b\x08\x18\x80\x62\xde\xd2\x36\x08\x3b\x51\x1e\xc0\x1a\xad\x0c\x46\x74\x87\xe4\x1b\x0c\x5a\xeb\x89\x3a\x92\x0b\xe8\x22\x5f\x65\x8f\x45\xb1\xcc\xd6\xcb\xc5\x79\xb1\x3e\xa9\x7f\x54\x5c\x88\xbd\xaa\xe2\x1c\x5a\x41\xc4\xb5\xb3\xbe\xaa\x97\x46\xec\x34\xca\x14\x7e\xb2\x56\xa3\xb8\xe8\x6c\xb4\x68\x37\x52\xa3\x38\xd0\xf4\x7a\xbe\xe1\x87\x16\xc4\x77\xc3\x34\x1e\x28\x85\x9f\xb5\x15\x71\x85\x8e\x23\x82\xf4\x8a\xe7\xcd\xed\x81\xd7\x7b\xdb\x1d\xdd\xce\xa5\x74\x48\x34\xd6\x33\xdb\x03\x9a\x2b\x35\xc7\x2c\xc3\x7d\x11\x03\x33\x87\x82\x31\xfb\x86\x76\x12\x80\x4d\xbc\x96\xc7\x2c\x6f\xfb\x01\xe4\x8b\x50\xea\xfb\xff\x6b\xd3\xbb\xf3\xdb\xa8\x80\x8b\x6c\xfa\x7b\xc4\xbb\xab\x2b\x08\x80\x6a\xf1\xfd\x8f\x1f\xbe\xa6\xbe\x52\x50\x57\x33\x63\x43\x29\xfc\x39\xfc\xf9\xeb\x2b\xdf\xe8\x76\x44\x47\x51\xf5\x97\x53\xca\x5a\x98\x0a\xb5\xad\xae\xba\xa5\x1a\x24\x16\x4d\x3b\x1a\xcb\x6b\xf2\x5f\x00\x00\x00\xff\xff\xfc\xf7\xc6\x26\x36\x07\x00\x00")

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

	info := bindataFileInfo{name: "schema.graphql", size: 1846, mode: os.FileMode(436), modTime: time.Unix(1584056358, 0)}
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
