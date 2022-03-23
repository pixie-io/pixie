// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_cron_scripts_table.down.sql
// 000001_create_cron_scripts_table.up.sql
// 000002_add_frequency_cron_scripts_table.down.sql
// 000002_add_frequency_cron_scripts_table.up.sql
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

var __000001_create_cron_scripts_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\x2e\xca\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\xb6\xe6\x02\x04\x00\x00\xff\xff\x4e\x01\x55\x60\x23\x00\x00\x00")

func _000001_create_cron_scripts_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_cron_scripts_tableDownSql,
		"000001_create_cron_scripts_table.down.sql",
	)
}

func _000001_create_cron_scripts_tableDownSql() (*asset, error) {
	bytes, err := _000001_create_cron_scripts_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_cron_scripts_table.down.sql", size: 35, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_cron_scripts_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x64\x92\x41\x8f\xda\x30\x10\x85\xef\xf9\x15\xef\xb8\x48\xec\xf6\xd2\x5b\x4f\xb4\xa4\x12\x6a\x96\x6e\x51\x72\x40\x55\x85\x4c\x32\xc1\xa3\x1a\x1b\x79\x6c\xd8\xfd\xf7\x95\x8d\xe9\xa6\xf4\x16\x67\x66\xbe\xf7\xe6\x69\xbe\x6c\xea\x45\x5b\xa3\x5d\x7c\x6e\x6a\xf4\xde\xd9\x9d\xf4\x9e\x4f\x41\xf0\x50\x01\x8f\x8f\x68\x35\x61\xb5\x84\x1b\x11\x34\xe5\x0e\x5c\x3b\x9e\x2a\x80\x07\x74\xdd\x6a\x89\x6e\xbd\xfa\xd1\xd5\x58\xd6\x5f\x17\x5d\xd3\x22\x46\x1e\x76\x07\xb2\xe4\x55\xa0\xdd\xf9\xe3\xc3\x6c\x7e\x85\x39\x7f\xd8\xf1\x00\x96\x0c\x73\xfe\x80\x8b\x76\xd0\x2a\xbd\x59\x0a\x18\x64\xd5\xde\xd0\x90\x04\xca\x40\x16\x59\x7f\x6f\xb1\xee\x9a\xa6\xb0\x7a\x67\x03\xd9\x20\xf9\x43\xb1\xbd\x32\x55\x1f\xa2\x32\x78\x79\x6d\x26\x36\x0b\xf7\xac\x7c\xaf\x95\xbf\xcd\xa7\x65\xe9\xf5\xe4\x49\x84\xd3\x56\x27\xea\x79\x64\x92\x64\x4f\xbb\x0b\xdc\x18\xc8\x66\x68\x99\x17\xed\xa2\x19\xe0\xa3\x45\x14\xb6\x07\xa8\x6b\x1e\xef\x90\x24\x76\xcf\xbd\x53\x35\x51\x02\xf9\x1d\x0f\x72\x8b\xc1\xb0\x84\x94\x6f\x29\x09\x2e\x9a\x7b\xfd\x4f\x22\x13\x65\x67\x9f\xb0\x1a\x41\xc7\x53\x78\x9b\x43\x89\xc4\x63\xb2\x1c\x52\x51\xe0\x2c\x94\x31\xef\x28\xb6\xb7\xa4\xb3\xb5\x89\x78\x8a\xf4\xe7\xaf\xe2\xaa\xb6\x67\xf6\xce\x1e\xc9\xe6\x94\x38\xe5\x9f\xcc\xa9\xbf\xd2\x7b\x42\x14\x1a\x10\x1c\x46\x36\xe6\x06\xbe\xfa\x9b\xa7\xa7\xc2\x76\xf1\xdc\x60\x74\xfe\xa8\x72\xea\xbd\xb3\x23\x1f\x04\xfb\xb7\x40\xaa\x08\xb5\xee\x37\xd9\xd4\x93\xa7\xa3\x90\xff\x90\xae\x20\xfd\xf8\x7f\x6b\x96\xb2\x31\xf6\xa4\x95\x19\x13\x34\x64\xc0\x14\x59\xae\x25\x75\x5f\x34\x05\x4d\xfe\xfe\x52\x53\x69\x72\x53\xb7\x81\xbd\x73\x86\x94\x9d\x57\x15\xf0\xb2\x59\x3d\x2f\x36\x5b\x7c\xab\xb7\x78\xe0\x61\x56\xcd\x3e\x55\x7f\x02\x00\x00\xff\xff\x68\xf4\x8f\x5d\x1b\x03\x00\x00")

func _000001_create_cron_scripts_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_cron_scripts_tableUpSql,
		"000001_create_cron_scripts_table.up.sql",
	)
}

func _000001_create_cron_scripts_tableUpSql() (*asset, error) {
	bytes, err := _000001_create_cron_scripts_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_cron_scripts_table.up.sql", size: 795, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_add_frequency_cron_scripts_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x2e\xca\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\xe6\x52\x50\x70\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x2b\x4a\x2d\x2c\x4d\xcd\x4b\xae\x8c\x2f\xb6\x06\x04\x00\x00\xff\xff\x07\xd8\x60\x78\x33\x00\x00\x00")

func _000002_add_frequency_cron_scripts_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_add_frequency_cron_scripts_tableDownSql,
		"000002_add_frequency_cron_scripts_table.down.sql",
	)
}

func _000002_add_frequency_cron_scripts_tableDownSql() (*asset, error) {
	bytes, err := _000002_add_frequency_cron_scripts_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_add_frequency_cron_scripts_table.down.sql", size: 51, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_add_frequency_cron_scripts_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x48\x2e\xca\xcf\x8b\x2f\x4e\x2e\xca\x2c\x28\x29\xe6\x52\x50\x70\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x2b\x4a\x2d\x2c\x4d\xcd\x4b\xae\x8c\x2f\x56\xc8\xcc\x2b\x49\x4d\x4f\x2d\xb2\x06\x04\x00\x00\xff\xff\x1d\x24\xc4\xf2\x3a\x00\x00\x00")

func _000002_add_frequency_cron_scripts_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_add_frequency_cron_scripts_tableUpSql,
		"000002_add_frequency_cron_scripts_table.up.sql",
	)
}

func _000002_add_frequency_cron_scripts_tableUpSql() (*asset, error) {
	bytes, err := _000002_add_frequency_cron_scripts_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_add_frequency_cron_scripts_table.up.sql", size: 58, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_cron_scripts_table.down.sql":        _000001_create_cron_scripts_tableDownSql,
	"000001_create_cron_scripts_table.up.sql":          _000001_create_cron_scripts_tableUpSql,
	"000002_add_frequency_cron_scripts_table.down.sql": _000002_add_frequency_cron_scripts_tableDownSql,
	"000002_add_frequency_cron_scripts_table.up.sql":   _000002_add_frequency_cron_scripts_tableUpSql,
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
	"000001_create_cron_scripts_table.down.sql":        &bintree{_000001_create_cron_scripts_tableDownSql, map[string]*bintree{}},
	"000001_create_cron_scripts_table.up.sql":          &bintree{_000001_create_cron_scripts_tableUpSql, map[string]*bintree{}},
	"000002_add_frequency_cron_scripts_table.down.sql": &bintree{_000002_add_frequency_cron_scripts_tableDownSql, map[string]*bintree{}},
	"000002_add_frequency_cron_scripts_table.up.sql":   &bintree{_000002_add_frequency_cron_scripts_tableUpSql, map[string]*bintree{}},
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
