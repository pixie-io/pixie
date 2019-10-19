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

var _schemaGraphql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x6c\x93\xcf\x8f\xeb\x34\x10\xc7\xef\xf9\x2b\xbe\xa8\x07\x40\xa2\x3d\x20\x78\x87\xdc\xaa\xb4\xb0\x11\xbb\xd9\xc7\xb6\xe5\x01\x97\x95\x1b\x4f\x1b\xab\x8e\x9d\xb5\xc7\xbb\x54\xe8\xfd\xef\xc8\x4e\xd2\xa6\x15\xa7\xc6\xd3\x99\xef\x7c\xe6\xd7\x0c\xdb\x46\x79\x1c\x94\x26\x48\xf2\xb5\x53\x7b\xf2\xe0\x86\xe0\xeb\x86\x5a\x81\x83\xb3\x6d\x7a\x2f\x3f\x97\xf0\xe4\xde\x55\x4d\x8b\x6c\x96\xcd\x50\xf2\xb7\x1e\xc6\x32\x94\x24\xa1\x7f\xc0\x3e\x30\x3e\x08\x86\x48\x82\x2d\x5a\x61\x82\xd0\xfa\x8c\x23\x19\x72\x82\x09\x7c\xee\xc8\xe3\x60\x5d\xd2\xdb\x9e\x3b\xda\xd4\x4e\x75\x8c\x5d\x99\xcd\xf0\xd1\x90\x01\x5f\x60\x94\x47\xe8\xa4\x60\x92\x8b\x1e\xb1\x16\x06\x7b\x82\xb4\x86\xb0\x3f\xc3\x05\x63\x94\x39\xe6\xd9\x0c\x38\x3a\xd1\x35\x6f\x7a\xde\x23\xcf\x53\x9e\x5e\x79\xcc\x3d\x67\x3f\x14\xb4\x18\x9c\x31\x9f\xdb\xc0\x5d\xe0\xd1\x2e\x17\xec\x13\x86\xaa\x1b\x7c\x28\xad\x27\xe0\x0d\x61\x70\x8e\xda\x3d\x20\x37\x82\x7b\xbf\x3d\xa1\x53\xf5\x89\x24\x42\x17\xd1\xa2\xfb\xae\x5c\x64\x43\x6f\x27\xfa\x29\xd2\xc3\x37\x36\x68\x09\xfa\x47\x79\x86\x32\x7d\xbb\x45\x4b\x90\xca\x51\xcd\xd6\x9d\x21\xa6\x43\xb8\x30\xc7\xf0\x45\x96\x0d\xa3\xf9\x37\x03\xde\x02\xb9\x73\x8e\xdf\xe3\x4f\x06\xb4\x81\x05\x2b\x6b\x72\x3c\x0d\x5f\xd9\xd7\x2c\x4b\xd0\x3b\x4f\xae\x34\x07\x9b\xc2\x94\xcc\x51\xae\xbe\xc9\x00\x23\x5a\xca\xb1\x61\xa7\xcc\x31\xbe\xa9\x15\x4a\x4f\x0d\x9d\xaa\x39\xb8\x89\xcf\xd7\x2c\x23\x13\x5a\x2c\x1d\xab\x83\xa8\x39\x0e\x32\x89\x02\xcb\xed\xeb\xae\xfa\xad\x7a\xfe\x52\x8d\xcf\xc7\xb2\xda\xfd\xf9\xba\x7c\x5a\x7d\xfa\x69\x34\xad\x96\x2f\x5f\xca\xea\xd6\x56\x3c\x57\xdb\x65\x59\xad\x5f\x5e\x37\xeb\xdb\xa0\x11\x3f\x55\x98\xd2\x04\x4f\x2e\xbf\x94\x13\x11\x6b\x1d\x3c\x47\x63\xd1\x7f\xdc\xd9\x0b\x6b\x0c\xd5\x7d\x5b\x8a\x7b\xd3\xd5\x57\x8d\x05\x7d\x27\x26\x95\xe5\x37\x75\x7e\x9f\xa3\x78\x2c\x47\xcb\xb5\x17\x83\xec\x86\x05\x07\x9f\x28\xe3\xf0\xe3\x49\x1d\x44\xd0\x0c\xcf\x71\x8f\xd4\x21\xde\x4b\xa3\xcc\x31\xae\xf7\xc9\xd8\x0f\xb3\xc8\x80\x3f\xfe\x7e\xdd\x4c\x1b\xd7\x87\x0e\x21\x1e\x0d\x09\xcd\xcd\x39\x46\x37\x24\x1c\xef\x49\xb0\x87\x70\x04\x47\x35\xa9\x77\x92\xb0\x06\x8e\x8e\x41\x0b\x07\x65\x98\xdc\xbb\xd0\x1e\xc2\xc8\xb8\x43\x49\x70\xe8\x44\x94\x73\xe4\x3b\x6b\x64\x84\x60\x0b\x47\x6f\x81\x3c\xfb\x2b\xc7\xc3\x7a\xf9\xb8\x7d\xf8\xeb\x8e\xa3\x3f\x08\x1b\x43\xa4\xf2\x75\xdf\x3e\x92\x11\x2a\xee\xe9\xaf\x2f\x9f\x0b\xd4\x97\xa6\x62\xef\x48\x9c\xfc\x22\x09\x34\xb6\x4b\x75\xc4\x83\x49\xaa\x31\x60\x04\x4a\xba\xb5\x6d\x09\x7b\x51\x9f\x60\x8d\x56\x86\x12\xba\x23\x1f\x5a\x8a\xe7\x31\x10\xf5\x24\x57\xd0\x55\xb9\x29\x9e\xab\x6a\x5d\x6c\xd7\xab\xcb\x9a\x4c\x36\xe0\x6e\xd1\x7d\x9a\x4d\x7e\x3b\xaa\xf8\x87\x16\x9e\x1f\xc6\xc6\x3e\xf9\x1c\xbf\x68\x2b\xfa\xd9\x4e\x25\x6f\x57\xa6\x17\xef\x96\x52\x3a\xf2\x7e\x7a\x2f\x6c\x4f\x64\x6e\xae\x25\xa9\x8c\xf7\x98\x02\x0b\x47\x82\xa9\xf8\x9f\xb5\xbd\x66\xbd\xae\xd9\x70\x5c\xc1\xdd\xdc\x25\xe0\x1b\xf1\xe3\xcf\x9f\xa6\xa9\xfe\x0b\x00\x00\xff\xff\xb6\x1d\x1c\xaf\xce\x05\x00\x00")

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

	info := bindataFileInfo{name: "schema.graphql", size: 1486, mode: os.FileMode(436), modTime: time.Unix(1571423349, 0)}
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
