// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// kratos_config.yaml
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

var _kratos_configYaml = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xac\x55\x5d\xaf\xdb\x44\x10\x7d\xf7\xaf\x18\xb9\x3c\x92\xf8\xe6\x56\x50\x69\x25\x04\x88\xf2\x50\xa9\x55\x2b\x50\x79\x41\x28\xda\x6b\x8f\xed\x69\xd6\xbb\xee\xcc\x38\x25\x2a\xfd\xef\x68\xbd\xb1\x63\xf2\x71\xaf\x40\xf8\x21\x92\xcf\x9c\x39\x73\x66\x67\xbc\x59\xad\x56\x99\xed\xe9\x37\x64\xa1\xe0\x0d\xec\x37\xd9\x8e\x7c\x65\xe0\xa7\xe0\x6b\x6a\xde\xd8\x3e\xeb\x50\x6d\x65\xd5\x9a\x0c\xc0\xdb\x0e\x0d\xec\xd8\x6a\x90\x55\x39\x52\xb2\x29\xf6\x0c\x0e\xb6\x73\x8e\xbc\x42\x45\x62\x1f\x1c\x02\x0f\x0e\x0d\xf9\x0a\xbd\x5a\xa5\xe0\x33\x38\xe6\xae\x0f\x9d\x33\xf0\x57\x06\x10\xf3\x7e\x74\x0e\xde\xff\xf2\xfa\x57\xe8\x06\x51\x78\x40\x10\x54\x20\x0f\xda\x22\xa0\xdf\x13\x07\xdf\xa1\x57\xd8\x5b\xa6\xa8\x2b\x40\x5e\x14\x6d\x05\xa1\x86\xe4\x62\x3d\x4a\x09\xba\x5a\x90\xf7\x54\xa2\x19\x01\x80\x0e\xb5\x0d\x95\x4c\xaf\x00\xbd\x15\xf9\x14\xb8\x3a\x21\x00\xe8\xa3\x6c\x65\x40\x79\xc0\x19\x77\xe4\x77\x4f\xb1\x6a\x17\x3e\x2d\xc4\xf7\xc8\x54\x53\x39\x36\x7b\x35\xb5\xb6\x4e\x30\x9b\x23\x8c\x65\xd8\x23\x1f\x6e\x97\x59\x50\x1b\x12\xe5\x0b\x69\x5b\x2b\xf2\x12\xb8\xde\x62\x7c\xda\x10\x76\x72\x0e\x02\xac\x2e\x90\x44\x35\x20\x28\x92\xc6\x06\xe0\x42\x33\xa5\x3a\xdc\xa3\x8b\xfe\x6c\x99\xce\xa1\xb5\xd2\x22\xcf\xd2\x96\x9b\xe0\xef\x97\x47\xce\xd6\x39\x74\x24\x9d\x81\xcd\x0c\x77\xd8\x05\x3e\x18\xd8\x3c\xdf\xdc\xbd\xb8\x9f\x61\x52\x4c\x5d\x8a\x81\x13\x2a\xd6\xe9\xd6\xa1\x6f\xb4\x35\xb0\xf9\x76\xc6\x77\x78\x58\xc2\x23\x4e\x71\xe1\x48\xe7\x53\xad\xb0\xb6\x83\xd3\xad\x94\x2d\x76\x76\x3b\xb0\x33\x50\x93\x43\x53\x14\x05\x6a\x59\xa4\x15\x2a\xd2\x6a\x16\x53\xf6\x3a\xd1\xd7\x1f\x24\xf8\xa4\x5b\x86\x81\xe9\x74\xd8\xd2\x69\x7f\x6a\xb2\x0c\xde\x63\x19\x7d\x6f\x07\x26\x33\x46\xc5\x14\x85\xa2\xa8\x89\x3f\x3f\x74\x96\x9c\xb8\x81\x7b\x64\xb3\xb9\xbb\xff\xa6\xf8\x5e\x76\xd4\x6f\x45\xdc\x76\xdc\x9b\xc3\x77\xc7\xc5\xba\xe6\x60\xfa\x5a\x3e\x1f\x0b\xe6\x5f\x51\x95\x1b\xc8\x5b\x4d\x65\x12\x55\xd6\x81\x0f\x6b\x69\x8b\x9e\x51\x50\x65\xea\xe9\xe3\x40\xe5\x4e\xd4\xb2\x16\x18\x6d\xac\xa6\x15\xb9\xda\x6d\xfe\xf5\x5c\x24\xa1\x53\x21\x53\x14\x31\xbe\x3a\x72\x03\x37\x45\xc5\xb6\xd6\xd5\xdd\x8b\xa3\x81\x67\xa7\x5c\x25\x75\x18\x33\xdf\x0b\xf2\x02\x3e\xf4\x23\x1a\x1e\x3e\x60\xa9\x27\xbc\xe7\xd0\x23\x2b\xa1\xe4\x66\xee\x32\xf2\xd9\x92\x4a\x6e\x3e\x2f\xb6\xf4\x96\xc8\x63\x42\x63\x6c\x6c\xfe\x02\x5e\x08\x8a\x32\xf9\xe6\x1f\x82\x63\xbc\x0e\xdc\x59\x8d\x8c\x24\x71\x41\x98\xbb\xfd\x79\xf5\xe6\x2a\xa1\x23\xff\x7a\xdc\xd3\xdc\xc0\xf3\x8b\xe8\x71\x6c\x69\x5c\x57\x0c\x02\xe4\x25\xe3\x38\x2b\xeb\xae\x13\x62\xeb\xc7\xa9\xde\x88\x03\xe4\x69\xdc\x35\x21\xe7\x67\xb7\xdd\xe9\xf9\x72\x81\x7d\x39\xf7\x0b\x90\x4f\x57\xd7\x2d\x2f\x7b\xb2\xa7\xe3\xba\x54\xcc\x1e\x7b\x3f\xab\x97\xf7\x2e\xee\xd0\xab\x97\xff\x65\x72\xf3\x60\xde\xd1\x9f\x84\x10\x85\xe0\xd5\xcb\xc7\xe7\xb3\x79\xc2\xcd\x5b\x6e\xfe\x17\x33\x6f\xb9\xf9\xb7\x5e\xb2\x1b\xbe\x72\xc6\x8f\x03\x31\xc6\xd9\xff\x7e\x6d\xe9\x17\xd8\x1f\xd9\xb9\xdc\x2c\x95\xdb\xaa\xa2\x78\x89\x59\xf7\x6e\xf9\x1d\xa5\xff\xad\x94\xf2\x77\x00\x00\x00\xff\xff\xe7\xcd\xb8\xd5\x34\x08\x00\x00")

func kratos_configYamlBytes() ([]byte, error) {
	return bindataRead(
		_kratos_configYaml,
		"kratos_config.yaml",
	)
}

func kratos_configYaml() (*asset, error) {
	bytes, err := kratos_configYamlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "kratos_config.yaml", size: 2100, mode: os.FileMode(436), modTime: time.Unix(1619552691, 0)}
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
	"kratos_config.yaml": kratos_configYaml,
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
	"kratos_config.yaml": &bintree{kratos_configYaml, map[string]*bintree{}},
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
