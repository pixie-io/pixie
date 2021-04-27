// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_dns_tables.down.sql
// 000001_create_dns_tables.up.sql
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

var __000001_create_dns_tablesDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\x2e\xce\x89\x4f\x4e\x2d\x2a\x29\xb6\xe6\xc2\x2a\x9f\x92\x57\x1c\x9f\x98\x92\x52\x94\x5a\x5c\x9c\x5a\x6c\xcd\x05\x08\x00\x00\xff\xff\xbd\x6d\xb7\xe6\x44\x00\x00\x00")

func _000001_create_dns_tablesDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_dns_tablesDownSql,
		"000001_create_dns_tables.down.sql",
	)
}

func _000001_create_dns_tablesDownSql() (*asset, error) {
	bytes, err := _000001_create_dns_tablesDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_dns_tables.down.sql", size: 68, mode: os.FileMode(436), modTime: time.Unix(1619552691, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_dns_tablesUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x8c\x91\xc1\x6a\x1b\x31\x10\x86\xef\x7a\x8a\x9f\x9c\x1c\x88\x43\xd2\x53\x4b\x4e\x6e\xab\xc2\x52\xdb\x31\x5e\x19\x92\x93\x91\xa5\x71\x57\xd4\x2b\xa5\x1a\xa9\xa5\x6f\x5f\x76\xbd\xf2\x76\xc1\x87\xdc\x84\x86\xef\x9f\x6f\x66\xbe\x6c\xe5\x42\x49\xc8\x17\x25\xd7\x75\xf5\xbc\x46\xf5\x0d\xeb\x67\x05\xf9\x52\xd5\xaa\xc6\x4d\xce\xce\xce\x03\xf3\xdb\xcd\x93\x10\xf3\x39\x54\xe3\x18\x49\x1f\x4e\x04\x13\x7c\xd2\xce\x33\x9c\x3f\x86\xd8\xea\xe4\x82\x87\x3e\x84\x9c\x10\x72\x44\x5d\x2f\x61\x28\x26\x86\xf6\x16\x7f\x1a\x67\x1a\x98\x53\xe6\x44\x91\xa1\x23\x21\xb3\xf3\x3f\x90\x1a\x6a\xef\xc5\xa0\xa1\x16\x9f\x97\x12\xcc\xa7\xfd\x99\x9c\x09\xa0\x6f\x4a\x30\x5e\xb7\x84\x63\x88\x1d\xd1\x07\xdf\x0b\x0c\xbf\xbf\x75\x34\x8d\x8e\xb3\xc7\x87\x87\xdb\xbb\x33\x32\x74\xda\x3b\x8b\x4e\xb8\x21\x38\x8b\x70\x3c\xc3\xe7\xda\x28\x30\xc6\x8d\xd4\x6e\x57\x7d\x2d\x59\x14\x53\x49\xe9\xdf\xc5\xa2\x8c\xd8\xa3\x5d\xa1\x88\x7c\x7c\xfc\xf4\xa1\x98\xfc\xa4\xbf\x05\xee\x9e\xd7\xd8\xee\x7f\x82\x8a\xdb\x27\x21\xa6\x4b\xb1\x9e\xf7\xda\xda\x48\xcc\x74\x59\xcc\x3b\xa6\xbc\x32\x56\x7f\xe0\xf5\x6e\xb9\x1c\x0c\x93\x6b\x69\x6f\x22\xe9\x44\x97\x1c\xa6\x5f\x99\xbc\x21\xf8\xdc\x1e\x28\x22\x05\x64\x1e\xf7\x6f\x3d\x63\xb0\xe9\x1a\x4c\x12\x54\xb5\x92\xb5\x5a\xac\x36\x43\xbc\x7b\x2b\xe2\x97\x25\xe6\x18\xc9\x27\x54\x9b\x12\x72\x45\xba\x54\xfe\x3b\x6e\x77\x5d\x01\x6c\xb6\xd5\x6a\xb1\x7d\xc5\x77\xf9\x3a\x1b\x47\xbb\x9b\x58\xf4\x2b\xfc\x17\x00\x00\xff\xff\x97\x0b\x23\x16\xdd\x02\x00\x00")

func _000001_create_dns_tablesUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_dns_tablesUpSql,
		"000001_create_dns_tables.up.sql",
	)
}

func _000001_create_dns_tablesUpSql() (*asset, error) {
	bytes, err := _000001_create_dns_tablesUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_dns_tables.up.sql", size: 733, mode: os.FileMode(436), modTime: time.Unix(1619552691, 0)}
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
	"000001_create_dns_tables.down.sql": _000001_create_dns_tablesDownSql,
	"000001_create_dns_tables.up.sql":   _000001_create_dns_tablesUpSql,
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
	"000001_create_dns_tables.down.sql": &bintree{_000001_create_dns_tablesDownSql, map[string]*bintree{}},
	"000001_create_dns_tables.up.sql":   &bintree{_000001_create_dns_tablesUpSql, map[string]*bintree{}},
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
