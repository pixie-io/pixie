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

var _kratos_configYaml = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\xac\x55\x5d\xaf\xdb\x44\x10\x7d\xf7\xaf\x18\xb9\x3c\x92\xf8\xe6\x56\x50\x69\x25\x04\x88\xf2\x50\xa9\x55\x2b\x50\x79\x41\x28\xda\x6b\x8f\xed\x69\xd6\xbb\xee\xcc\x38\x25\x2a\xfd\xef\x68\xbd\xb1\x63\xf2\x71\xaf\x40\xf8\x21\x92\xcf\x9c\x39\x73\x66\x67\xbc\x59\xad\x56\x99\xed\xe9\x37\x64\xa1\xe0\x0d\xec\x37\xd9\x8e\x7c\x65\xe0\xa7\xe0\x6b\x6a\xde\xd8\x3e\xeb\x50\x6d\x65\xd5\x9a\x0c\xc0\xdb\x0e\x0d\xec\xd8\x6a\x90\x55\x39\x52\xb2\x29\x26\x43\x6f\x20\x6f\x29\xcf\x00\x9e\xc1\xc1\x76\xce\x91\x57\xa8\x48\xec\x83\x43\xe0\xc1\xa1\x21\x5f\xa1\x57\xab\x14\x7c\x06\x47\x9d\xf5\xa1\x73\x06\xfe\xca\x00\x62\xde\x8f\xce\xc1\xfb\x5f\x5e\xff\x0a\xdd\x20\x0a\x0f\x08\x82\x0a\xe4\x41\x5b\x04\xf4\x7b\xe2\xe0\x3b\xf4\x0a\x7b\xcb\x14\x75\x05\xc8\x8b\xa2\xad\x20\xd4\x90\x1c\xad\x47\x29\x41\x57\x0b\xf2\x9e\x4a\x34\x23\x00\xd0\xa1\xb6\xa1\x92\xe9\x15\xa0\xb7\x22\x9f\x02\x57\x27\x04\x00\x7d\x94\xad\x0c\x28\x0f\x38\xe3\x8e\xfc\xee\x29\x56\xed\xc2\xa7\x85\xf8\x1e\x99\x6a\x2a\xc7\x66\xaf\xa6\xd6\xd6\x09\x66\x73\x84\xb1\x0c\x7b\xe4\xc3\xed\x32\x0b\x6a\x43\xa2\x7c\x21\x6d\x6b\x45\x5e\x02\xd7\x5b\x8c\x4f\x1b\xc2\x4e\xce\x41\x80\xd5\x05\x92\xa8\x06\x04\x45\xd2\xd8\x00\x5c\x68\xa6\x54\x87\x7b\x74\xd1\x9f\x2d\xd3\x39\xb4\x56\x5a\xe4\x59\xda\x72\x13\xfc\xfd\xf2\xc8\xd9\x3a\x87\x8e\xa4\x33\xb0\x99\xe1\x0e\xbb\xc0\x07\x03\x9b\xe7\x9b\xbb\x17\xf7\x33\x4c\x8a\xa9\x4b\x31\x70\x42\xc5\x3a\xdd\x3a\xf4\x8d\xb6\x06\x36\xdf\xce\xf8\x0e\x0f\x4b\x78\xc4\x29\x2e\x1c\xe9\x7c\xaa\x15\xd6\x76\x70\xba\x95\xb2\xc5\xce\x6e\x07\x76\x06\x6a\x72\x68\x8a\xa2\x40\x2d\x8b\xb4\x42\x45\x5a\xcd\x62\xca\x5e\x27\xfa\xfa\x83\x04\x9f\x74\xcb\x30\x30\x9d\x0e\x5b\x3a\xed\x4f\x4d\x96\xc1\x7b\x2c\xa3\xef\xed\xc0\x64\xc6\xa8\x98\xa2\x50\x14\x35\xf1\xe7\x87\xce\x92\x13\x37\x70\x8f\x6c\x36\x77\xf7\xdf\x14\xdf\xcb\x8e\xfa\xad\x88\xdb\x8e\x7b\x73\xf8\xee\xb8\x58\xd7\x1c\x4c\x5f\xcb\xe7\x63\xc1\xfc\x2b\xaa\xf2\xf8\xed\x69\x2a\x93\xa8\xb2\x0e\x7c\x58\x4b\x5b\xf4\x8c\x82\x2a\x53\x4f\x1f\x07\x2a\x77\xa2\x96\xb5\xc0\x68\x63\x35\xad\xc8\xd5\x6e\xf3\xaf\xe7\x22\x09\x9d\x0a\x99\xa2\x88\xf1\xd5\x91\x1b\xb8\x29\x2a\xb6\xb5\xae\xee\x5e\x1c\x0d\x3c\x3b\xe5\x2a\xa9\xc3\x98\xf9\x5e\x90\x17\xf0\xa1\x1f\xd1\xf0\xf0\x01\x4b\x3d\xe1\x3d\x87\x1e\x59\x09\x25\x37\x73\x97\x91\xcf\x96\x54\x72\xf3\x79\xb1\xa5\xb7\x44\x1e\x13\x1a\x63\x63\xf3\x17\xf0\x42\x50\x94\xc9\x37\xff\x10\x1c\xe3\x75\xe0\xce\x6a\x64\x24\x89\x0b\xc2\xdc\xed\xcf\xab\x37\x57\x09\x1d\xf9\xd7\xe3\x9e\xe6\x06\x9e\x5f\x44\x8f\x63\x4b\xe3\xba\x62\x10\x20\x2f\x19\xc7\x59\x59\x77\x9d\x10\x5b\x3f\x4e\xf5\x46\x1c\x20\x4f\xe3\xae\x09\x39\x3f\xbb\xed\x4e\xcf\x97\x0b\xec\xcb\xb9\x5f\x80\x7c\xba\xba\x6e\x79\xd9\x93\x3d\x1d\xd7\xa5\x62\xf6\xd8\xfb\x59\xbd\xbc\x77\x71\x87\x5e\xbd\xfc\x2f\x93\x9b\x07\xf3\x8e\xfe\x24\x84\x28\x04\xaf\x5e\x3e\x3e\x9f\xcd\x13\x6e\xde\x72\xf3\xbf\x98\x79\xcb\xcd\xbf\xf5\x92\xdd\xf0\x95\x33\x7e\x1c\x88\x31\xce\xfe\xf7\x6b\x4b\xbf\xc0\xfe\xc8\xce\xe5\x66\xa9\xdc\x56\x15\xc5\x4b\xcc\xba\x77\xcb\xef\x28\xfd\x6f\xa5\x94\xbf\x03\x00\x00\xff\xff\x02\xfb\xe0\x43\x40\x08\x00\x00")

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

	info := bindataFileInfo{name: "kratos_config.yaml", size: 2112, mode: os.FileMode(436), modTime: time.Unix(1618072975, 0)}
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
