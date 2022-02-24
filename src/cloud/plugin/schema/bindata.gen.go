// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_plugin_releases_table.down.sql
// 000001_create_plugin_releases_table.up.sql
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

var __000001_create_plugin_releases_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\xcd\x49\x4d\x2c\x4e\x2d\xb6\xe6\x02\x04\x00\x00\xff\xff\x04\x3a\xd0\x5c\x26\x00\x00\x00")

func _000001_create_plugin_releases_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_plugin_releases_tableDownSql,
		"000001_create_plugin_releases_table.down.sql",
	)
}

func _000001_create_plugin_releases_tableDownSql() (*asset, error) {
	bytes, err := _000001_create_plugin_releases_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_plugin_releases_table.down.sql", size: 38, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_plugin_releases_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x84\x91\x41\x6f\xe2\x30\x10\x85\xef\xf9\x15\xef\x08\x52\x58\xed\xae\xf6\xb6\xa7\xec\x2a\xda\x45\x05\x4a\x21\x50\x71\x8a\x26\xf1\x40\x2c\x25\x76\x6a\x4f\xa0\xfd\xf7\x95\x21\x20\x40\x55\x7b\xb3\xfc\xfc\xbe\xf1\x7b\xf3\x77\x91\x26\x59\x8a\x2c\xf9\x33\x49\xd1\xd6\xdd\x4e\x9b\xdc\x71\xcd\xe4\xd9\x63\x10\x01\xa3\x11\xfe\x77\x0d\x99\x91\x63\x52\x54\xd4\x0c\x43\x0d\x63\x6b\x1d\xa4\xe2\xde\xf2\x2d\xc2\xe9\x7a\x4f\xae\xac\xc8\x0d\x7e\x7c\xff\xf9\x6b\x88\xd9\x63\x86\xd9\x6a\x32\x89\x4f\x9c\x04\x9d\xd1\x2f\x1d\x43\x2b\x36\xa2\xb7\x9a\xdd\x1d\x27\x86\x6f\xb9\x0c\x8a\x42\xf1\x76\x25\xe0\xe0\xb4\xb0\x0b\x73\xb4\xfa\x6a\x8a\x62\x5f\x3a\xdd\x8a\xb6\x06\x54\xd8\x4e\xee\x7e\x7a\xad\xdf\xa0\x7a\xc2\x72\xfd\xef\xf2\xaf\xda\xee\x2c\xc4\xa2\xf3\x1f\x65\x3e\xa9\xfc\x2a\xbd\x33\xab\x18\x9e\x9b\x35\x3b\xec\xd9\xf9\x30\xc0\x6e\xaf\x63\xf4\xd5\x06\xef\xf9\xc1\xa7\x61\x02\x50\x74\xc3\x5e\xa8\x69\x41\x82\x43\xa5\xcb\xea\xa6\x18\xf2\xe8\x5a\x45\xc2\x2a\x50\xfb\x63\x4e\x82\x6c\x3c\x4d\x97\x59\x32\x9d\xf7\xac\xe7\x8a\xa5\x62\x07\x45\x42\x70\x2c\x61\x07\xd6\x40\x7b\xb0\x09\x8b\x55\x77\x01\x61\x1d\x8c\x95\x63\x63\x24\x94\x5f\x2c\xf9\xf9\x7d\x61\x6d\xcd\x64\xe2\x28\x02\xe6\x8b\xf1\x34\x59\x6c\xf0\x90\x6e\x30\xd0\x2a\x3e\xe7\x3b\x96\xba\x9a\x8d\x9f\x56\xe9\xed\x7d\x34\xfc\x1d\xbd\x07\x00\x00\xff\xff\xad\x69\xfe\xd8\x7e\x02\x00\x00")

func _000001_create_plugin_releases_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_plugin_releases_tableUpSql,
		"000001_create_plugin_releases_table.up.sql",
	)
}

func _000001_create_plugin_releases_tableUpSql() (*asset, error) {
	bytes, err := _000001_create_plugin_releases_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_plugin_releases_table.up.sql", size: 638, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_plugin_releases_table.down.sql": _000001_create_plugin_releases_tableDownSql,
	"000001_create_plugin_releases_table.up.sql":   _000001_create_plugin_releases_tableUpSql,
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
	"000001_create_plugin_releases_table.down.sql": &bintree{_000001_create_plugin_releases_tableDownSql, map[string]*bintree{}},
	"000001_create_plugin_releases_table.up.sql":   &bintree{_000001_create_plugin_releases_tableUpSql, map[string]*bintree{}},
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
