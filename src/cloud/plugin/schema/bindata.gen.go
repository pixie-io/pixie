// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_plugin_releases_table.down.sql
// 000001_create_plugin_releases_table.up.sql
// 000002_create_retention_releases_table.down.sql
// 000002_create_retention_releases_table.up.sql
// 000003_create_org_retention_table.down.sql
// 000003_create_org_retention_table.up.sql
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

var __000002_create_retention_releases_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x48\x49\x2c\x49\x8c\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\xc8\x29\x4d\xcf\xcc\x8b\x2f\x4a\xcd\x49\x4d\x2c\x4e\x2d\xb6\xe6\x02\x04\x00\x00\xff\xff\xd6\x40\x79\xfe\x35\x00\x00\x00")

func _000002_create_retention_releases_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_create_retention_releases_tableDownSql,
		"000002_create_retention_releases_table.down.sql",
	)
}

func _000002_create_retention_releases_tableDownSql() (*asset, error) {
	bytes, err := _000002_create_retention_releases_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_create_retention_releases_table.down.sql", size: 53, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_create_retention_releases_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x8c\x93\x5f\x6f\xd3\x30\x14\xc5\xdf\xf3\x29\xce\xdb\x3a\x29\x9b\xf8\xb7\xbd\xf0\x54\x86\x87\x06\x25\x85\xac\x05\x4d\x08\x45\x4e\x7c\xd3\x18\xa5\x76\xe6\x3f\xed\x2a\xc4\x77\x47\x76\x93\x2e\x54\xaa\xc4\x5b\xeb\xf8\xde\xf3\xbb\xe7\x5c\xdf\xe4\x6c\xba\x60\x58\x4c\xdf\xcd\x18\x04\x77\xbc\x30\xe4\x48\x39\xa9\x55\xd1\xb5\x7e\x25\x55\x61\xa8\x25\x6e\xc9\x62\x92\x00\x17\x17\x58\x34\x04\xaf\xe4\xa3\x27\x48\x11\xae\xd6\x92\x0c\x6a\x6d\xe0\x1a\xc2\xbe\xe8\x32\x41\xff\xab\x90\x02\x1b\x6e\xaa\x86\x9b\xc9\xcb\x17\xaf\xde\x9c\x23\x9b\x2f\x90\x2d\x67\xb3\xf4\xb9\x9d\xa5\xf5\x37\x32\xd8\x90\xb1\x52\x2b\xe8\x7a\xd4\x0a\xbd\x3e\x5c\xc3\x5d\x38\xb7\x84\x4a\xab\x5a\xae\xbc\xe1\x81\xd3\xa2\xa4\x56\xab\x15\x9c\x0e\xb2\x43\x93\xff\x10\x75\x41\xe9\xa8\xd7\xa0\x02\x6f\xc9\x40\x11\x09\x0b\xa7\x61\x3b\xaa\x64\xbd\x83\x54\xd0\x46\x90\x09\x67\x43\x25\xc5\xfb\xc1\x3c\x1c\xcc\x1b\xf9\x70\x24\xf0\xcb\x6a\x75\xc0\xc1\x7b\x76\x3b\x5d\xce\x16\x38\xfb\xfd\xe7\xac\x67\x9b\xa2\x95\x36\xa2\x75\x86\x02\xa4\xad\x8c\xec\x9c\x4d\xc3\xff\xad\x91\xce\x91\x42\xb9\xdb\x5b\x64\xf4\x46\x06\x9c\x6d\x23\xab\xe6\x99\xbb\xe2\x0a\xa4\x78\xd9\xd2\x25\x18\xaf\x1a\x7c\xbc\x9f\x67\x90\x16\xf4\xd4\x51\xe5\x48\xf4\xfc\x8e\x4b\x05\xc5\xd7\x94\xf6\x2a\x29\xb8\x12\x10\x54\x73\xdf\x3a\xd4\x86\x1e\x3d\xa9\x6a\x17\xf3\x8c\x34\x45\x4f\x13\xe7\xf8\xf1\xf3\xc0\xbc\xcc\x67\x3d\x44\xa7\xa5\x72\xd1\x34\x8e\x8e\xaf\x06\x48\xa9\x56\x10\xba\xf2\x6b\x52\x2e\x5a\x01\x5e\x6a\xef\xc6\x49\x0f\xd3\x9c\xd9\xd3\x6e\xfe\xd3\xa2\xf0\xa6\x3d\x04\x7d\x7d\x75\xf5\xfa\xfa\x7c\x14\xf0\x30\x05\x3d\x75\xda\x38\x90\x12\x11\xad\xc7\x8c\x0a\xb6\xd1\xbe\x15\x28\xc3\x36\x28\xd7\x6f\x50\x5f\x57\xec\xeb\x4e\x6b\x7c\x6f\xc8\x35\x64\xa2\xe3\x36\x5a\x3e\x6c\x09\x47\xe5\xad\xd3\xeb\xe8\x8a\xd3\x43\x3a\x3a\xa8\x88\x30\xb1\x34\x43\xaa\x41\x90\xb7\xad\xde\x16\xfb\x92\xb1\x6a\xa9\x75\x4b\x5c\xa5\x49\x02\x7c\xc9\xef\x3e\x4f\xf3\x07\x7c\x62\x0f\x98\x1c\x9e\x56\x3a\xac\x7b\x64\x5a\x66\x77\x5f\x97\xec\xe4\xe7\xdb\x79\xce\xee\x3e\x64\x27\x5b\x20\x67\xb7\x2c\x67\xd9\x0d\xbb\xc7\xd1\xdb\x9f\x8c\xef\x25\xe7\x6f\x93\xbf\x01\x00\x00\xff\xff\x5e\x64\xbc\x09\x38\x04\x00\x00")

func _000002_create_retention_releases_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_create_retention_releases_tableUpSql,
		"000002_create_retention_releases_table.up.sql",
	)
}

func _000002_create_retention_releases_tableUpSql() (*asset, error) {
	bytes, err := _000002_create_retention_releases_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_create_retention_releases_table.up.sql", size: 1080, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_create_org_retention_tableDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\xc8\x2f\x4a\x8f\x4f\x49\x2c\x49\x8c\x2f\x4a\x2d\x49\xcd\x2b\xc9\xcc\xcf\x8b\x2f\xc8\x29\x4d\xcf\xcc\x2b\xb6\xe6\x02\x04\x00\x00\xff\xff\x00\xb0\xba\xc2\x31\x00\x00\x00")

func _000003_create_org_retention_tableDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_create_org_retention_tableDownSql,
		"000003_create_org_retention_table.down.sql",
	)
}

func _000003_create_org_retention_tableDownSql() (*asset, error) {
	bytes, err := _000003_create_org_retention_tableDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_create_org_retention_table.down.sql", size: 49, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_create_org_retention_tableUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x7c\x91\xcd\x6e\xea\x30\x10\x85\xf7\x79\x8a\xb3\x4c\xa4\xc0\xfd\xd1\xdd\xdd\x15\x05\x53\xa5\xa5\xa1\x0d\xc9\x82\x55\x64\x92\x09\xb6\x14\xd9\xc8\x4e\x40\xbc\x7d\xe5\xfc\x00\x45\xb4\x3b\x5b\x73\xe6\x7c\x33\x67\xe6\x09\x9b\xa5\x0c\xe9\xec\x69\xc5\xa0\xcd\x3e\x2f\x79\xc3\x73\x43\x0d\xa9\x46\x6a\x95\x1f\xea\x76\x2f\x95\x85\xef\x01\x93\x49\xa7\x90\x25\xa4\x45\x23\xc8\xfd\x70\x12\x1a\x82\xbb\xbf\xb4\xe8\xd5\x20\xc5\x77\x35\x95\xbf\x4a\x69\xbb\xc7\xd4\xc3\xd8\x99\x65\xd1\x02\xf1\x3a\x45\x9c\xad\x56\x61\x6f\xda\x77\xdd\xf8\x46\x0b\xe8\xaa\x7b\x0d\x86\x27\x21\x0b\x71\x41\x3a\xdc\x23\xc4\xd5\xe7\xc8\x4d\x21\xb8\xf1\xff\xfc\xfe\xfb\x2f\xb8\xc7\x1d\xc9\x58\xa9\x15\xec\x81\x0a\x59\x49\xb2\x83\xfd\xc0\x32\x54\x13\xb7\xf4\x88\xe6\x20\x63\xf7\x8f\x88\xb9\x56\x95\xdc\xb7\x86\xbb\x0c\x2d\x0a\xad\x1a\xee\x52\x74\x9e\xad\x25\x33\x29\x06\x01\xb9\x59\xeb\x96\x2c\x2a\x6d\x6e\x36\x9e\x22\x75\x79\x5a\xa1\xdb\xba\x44\xa5\xeb\x5a\x9f\xba\x72\xf1\xd5\x79\xdc\xa1\x84\x54\x78\x78\xba\x7c\xd8\xc7\x4e\xfb\xd1\x52\x41\x3d\xd2\x85\xcd\xdd\xad\x0a\x73\x3e\x34\x54\xe2\x65\xb3\x8e\x9d\xe8\x0e\xb1\x3b\x37\xc4\x43\xcf\x03\xde\x93\xe8\x6d\x96\x6c\xf1\xca\xb6\xf0\xfb\x7b\x86\xd7\xd0\x03\xb7\x7b\x16\x47\x1f\x19\xfb\xae\xba\x5c\x27\x2c\x7a\x8e\x7b\x83\x4b\x29\x1c\x33\x0d\x90\xb0\x25\x4b\x58\x3c\x67\x1b\xdc\x4d\xef\xdf\xea\xbc\xe0\xbf\xf7\x19\x00\x00\xff\xff\x07\xa0\x1b\xd3\xb9\x02\x00\x00")

func _000003_create_org_retention_tableUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_create_org_retention_tableUpSql,
		"000003_create_org_retention_table.up.sql",
	)
}

func _000003_create_org_retention_tableUpSql() (*asset, error) {
	bytes, err := _000003_create_org_retention_tableUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_create_org_retention_table.up.sql", size: 697, mode: os.FileMode(436), modTime: time.Unix(1, 0)}
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
	"000001_create_plugin_releases_table.down.sql":    _000001_create_plugin_releases_tableDownSql,
	"000001_create_plugin_releases_table.up.sql":      _000001_create_plugin_releases_tableUpSql,
	"000002_create_retention_releases_table.down.sql": _000002_create_retention_releases_tableDownSql,
	"000002_create_retention_releases_table.up.sql":   _000002_create_retention_releases_tableUpSql,
	"000003_create_org_retention_table.down.sql":      _000003_create_org_retention_tableDownSql,
	"000003_create_org_retention_table.up.sql":        _000003_create_org_retention_tableUpSql,
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
	"000001_create_plugin_releases_table.down.sql":    &bintree{_000001_create_plugin_releases_tableDownSql, map[string]*bintree{}},
	"000001_create_plugin_releases_table.up.sql":      &bintree{_000001_create_plugin_releases_tableUpSql, map[string]*bintree{}},
	"000002_create_retention_releases_table.down.sql": &bintree{_000002_create_retention_releases_tableDownSql, map[string]*bintree{}},
	"000002_create_retention_releases_table.up.sql":   &bintree{_000002_create_retention_releases_tableUpSql, map[string]*bintree{}},
	"000003_create_org_retention_table.down.sql":      &bintree{_000003_create_org_retention_tableDownSql, map[string]*bintree{}},
	"000003_create_org_retention_table.up.sql":        &bintree{_000003_create_org_retention_tableUpSql, map[string]*bintree{}},
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
