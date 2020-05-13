// Code generated for package schema by go-bindata DO NOT EDIT. (@generated)
// sources:
// 000001_create_org_user_tables.down.sql
// 000001_create_org_user_tables.up.sql
// 000002_add_unique_constraint_email.down.sql
// 000002_add_unique_constraint_email.up.sql
// 000003_add_profile_picture.down.sql
// 000003_add_profile_picture.up.sql
// 000004_add_updated_created_at.down.sql
// 000004_add_updated_created_at.up.sql
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

var __000001_create_org_user_tablesDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\x09\xf2\x0f\x50\x08\x71\x74\xf2\x71\x55\xf0\x74\x53\x70\x8d\xf0\x0c\x0e\x09\x56\x28\x2d\x4e\x2d\x2a\xb6\xe6\xc2\x2a\x97\x5f\x94\x5e\x6c\xcd\x05\x08\x00\x00\xff\xff\x93\xee\xc5\x1a\x37\x00\x00\x00")

func _000001_create_org_user_tablesDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_org_user_tablesDownSql,
		"000001_create_org_user_tables.down.sql",
	)
}

func _000001_create_org_user_tablesDownSql() (*asset, error) {
	bytes, err := _000001_create_org_user_tablesDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_org_user_tables.down.sql", size: 55, mode: os.FileMode(436), modTime: time.Unix(1566246369, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000001_create_org_user_tablesUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x9c\x90\x4f\x4b\xc3\x30\x18\xc6\xef\xf9\x14\x0f\x3b\x35\xa0\x30\x41\x4f\x9e\xea\xf6\x56\x82\x33\xd3\x34\x81\xed\x54\x82\x8d\x33\xb0\xb6\x92\xac\xfb\xfc\xf2\x6a\xf5\xb0\xb1\x8b\xc7\x3c\x7f\xc2\xef\x79\x17\x86\x4a\x4b\xa0\x8d\x25\x5d\xab\xb5\x86\xaa\xa0\xd7\x16\xb4\x51\xb5\xad\x31\x1b\xc7\xd8\x5e\x0f\x39\x7f\xce\xee\x85\x98\xc2\xb6\x7c\x58\x11\x86\xb4\xcb\x28\x04\x10\x5b\x38\xa7\x96\x70\x5a\xbd\x3a\xc2\x92\xaa\xd2\xad\x2c\xb8\xd9\xec\x42\x1f\x92\x3f\x84\xe6\x78\x5b\xc8\x2b\x01\x6e\x35\xbd\xef\x02\x8e\x3e\xbd\x7d\xf8\x54\xdc\xcd\xe5\xd4\x64\xbb\x1d\x3a\x1f\xfb\xcb\x09\x01\xbc\x18\xf5\x5c\x9a\x2d\x9e\x68\x5b\xc4\x56\x0a\x79\x0a\x36\xe6\x90\xfe\x49\x36\x15\xf8\xc9\xbf\x9c\x62\xb0\xfe\x1e\x53\x3e\x9c\x01\xb2\xb3\xf7\x17\x8c\xd0\xf9\xb8\xff\x13\x6f\xe6\xac\x9e\x0f\xe1\x64\xb5\x36\xa4\x1e\x35\x4b\x28\x7e\x80\x24\x0c\x55\x64\x48\x2f\xa8\xfe\xbe\xf9\xef\xe8\xaf\x00\x00\x00\xff\xff\xb4\x85\x91\x1a\xba\x01\x00\x00")

func _000001_create_org_user_tablesUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000001_create_org_user_tablesUpSql,
		"000001_create_org_user_tables.up.sql",
	)
}

func _000001_create_org_user_tablesUpSql() (*asset, error) {
	bytes, err := _000001_create_org_user_tablesUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000001_create_org_user_tables.up.sql", size: 442, mode: os.FileMode(436), modTime: time.Unix(1566499465, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_add_unique_constraint_emailDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x0b\x0e\x09\x72\xf4\xf4\x0b\x51\x48\xcd\x4d\xcc\xcc\x89\x2f\xcd\xcb\x2c\x2c\x4d\xb5\xe6\x02\x04\x00\x00\xff\xff\x1a\x07\x06\xa9\x30\x00\x00\x00")

func _000002_add_unique_constraint_emailDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_add_unique_constraint_emailDownSql,
		"000002_add_unique_constraint_email.down.sql",
	)
}

func _000002_add_unique_constraint_emailDownSql() (*asset, error) {
	bytes, err := _000002_add_unique_constraint_emailDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_add_unique_constraint_email.down.sql", size: 48, mode: os.FileMode(436), modTime: time.Unix(1575417868, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000002_add_unique_constraint_emailUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x74\x71\x51\x70\xf6\xf7\x0b\x0e\x09\x72\xf4\xf4\x0b\x51\x48\xcd\x4d\xcc\xcc\x89\x2f\xcd\xcb\x2c\x2c\x4d\x55\x08\xf5\xf3\x0c\x0c\x75\x55\xd0\x00\x0b\x6a\x5a\x73\x01\x02\x00\x00\xff\xff\x0b\x8e\x6b\xf0\x3e\x00\x00\x00")

func _000002_add_unique_constraint_emailUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000002_add_unique_constraint_emailUpSql,
		"000002_add_unique_constraint_email.up.sql",
	)
}

func _000002_add_unique_constraint_emailUpSql() (*asset, error) {
	bytes, err := _000002_add_unique_constraint_emailUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000002_add_unique_constraint_email.up.sql", size: 62, mode: os.FileMode(436), modTime: time.Unix(1570640344, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_add_profile_pictureDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x28\x28\xca\x4f\xcb\xcc\x49\x8d\x2f\xc8\x4c\x2e\x29\x2d\x4a\xb5\xe6\x02\x04\x00\x00\xff\xff\x90\xf5\xcb\x8e\x2f\x00\x00\x00")

func _000003_add_profile_pictureDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_add_profile_pictureDownSql,
		"000003_add_profile_picture.down.sql",
	)
}

func _000003_add_profile_pictureDownSql() (*asset, error) {
	bytes, err := _000003_add_profile_pictureDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_add_profile_picture.down.sql", size: 47, mode: os.FileMode(436), modTime: time.Unix(1589389742, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000003_add_profile_pictureUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x74\x71\x51\x70\xf6\xf7\x09\xf5\xf5\x53\x50\x28\x28\xca\x4f\xcb\xcc\x49\x8d\x2f\xc8\x4c\x2e\x29\x2d\x4a\x55\x08\x73\x0c\x72\xf6\x70\x0c\xd2\x30\x34\x30\xd0\xb4\xe6\x02\x04\x00\x00\xff\xff\x5c\xe5\xa7\x74\x3c\x00\x00\x00")

func _000003_add_profile_pictureUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000003_add_profile_pictureUpSql,
		"000003_add_profile_picture.up.sql",
	)
}

func _000003_add_profile_pictureUpSql() (*asset, error) {
	bytes, err := _000003_add_profile_pictureUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000003_add_profile_picture.up.sql", size: 60, mode: os.FileMode(436), modTime: time.Unix(1589389742, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000004_add_updated_created_atDownSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x72\xf4\x09\x71\x0d\x52\x08\x71\x74\xf2\x71\x55\x28\x2d\x4e\x2d\x2a\xe6\x72\x09\xf2\x0f\x50\x70\xf6\xf7\x09\xf5\xf5\x53\x48\x2e\x4a\x4d\x2c\x49\x4d\x89\x4f\x2c\xb1\xe6\x82\x48\x84\x04\x79\xba\xbb\xbb\x06\x29\x78\xba\x29\xb8\x46\x78\x06\x87\x04\x2b\x94\x16\xa4\x24\x96\xa4\xc6\x83\x35\xc7\x43\x38\x20\x0d\x0a\xfe\x7e\x10\x03\xad\xb9\x00\x01\x00\x00\xff\xff\x0f\x2e\xd7\x9f\x64\x00\x00\x00")

func _000004_add_updated_created_atDownSqlBytes() ([]byte, error) {
	return bindataRead(
		__000004_add_updated_created_atDownSql,
		"000004_add_updated_created_at.down.sql",
	)
}

func _000004_add_updated_created_atDownSql() (*asset, error) {
	bytes, err := _000004_add_updated_created_atDownSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000004_add_updated_created_at.down.sql", size: 100, mode: os.FileMode(436), modTime: time.Unix(1589390009, 0)}
	a := &asset{bytes: bytes, info: info}
	return a, nil
}

var __000004_add_updated_created_atUpSql = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x94\x90\xc1\x8a\xb3\x30\x14\x85\xf7\x79\x8a\xb3\x10\xda\x6e\xfe\x17\x28\xff\x22\x4d\xae\x8e\xa0\x89\x5c\x6f\xe8\xec\x8a\x4c\xc5\x8d\x74\x9c\x5a\x99\xd7\x1f\x54\x44\x61\xdc\x8c\x1b\xe1\x26\xdf\xfd\xce\x89\xce\x84\x18\xa2\x2f\x19\x61\xe8\xeb\x67\x0f\x6d\x2d\x8c\xcf\x42\xee\xf0\xf1\xac\xab\x57\x7d\xbf\x55\x2f\x48\x9a\x53\x29\x3a\x2f\xce\x6a\x07\x99\x26\xbf\xa1\x92\x04\x96\x62\x1d\x32\xc1\xe3\xf3\xfb\x78\xda\x85\x57\xdf\xd0\xdd\xff\xee\xdb\x40\x3b\x3e\x65\x98\xb4\x10\x3c\x83\xa9\xc8\xb4\x21\xc4\xc1\x19\x49\xfd\x42\xde\xd6\x05\xc7\x93\x02\x98\x24\xb0\x2b\x21\x9c\x26\x09\x31\x74\x89\x28\x52\xc0\x85\x92\xd4\x29\x4c\x9f\xa3\xeb\xbf\x8d\xf7\xff\x62\x9b\x4f\xe7\x0d\xe3\xa5\x71\x42\xce\x8e\xbf\x28\x42\x5b\x3d\x9a\xa1\x6a\x6a\x1c\xba\xb6\x6b\xfa\xaf\xf6\xb0\xe6\x5b\x6c\x4b\xa6\xb1\xe7\x26\x99\xba\x50\xec\x99\x10\x0a\x3b\xb5\x71\xf3\x4b\xa8\xd8\x33\x48\x9b\x37\xb0\xbf\x82\xde\xc9\x04\x21\x14\xec\x0d\xd9\xc0\xb4\xd7\xf0\xfc\x13\x00\x00\xff\xff\x51\x8a\x4d\x52\xf2\x01\x00\x00")

func _000004_add_updated_created_atUpSqlBytes() ([]byte, error) {
	return bindataRead(
		__000004_add_updated_created_atUpSql,
		"000004_add_updated_created_at.up.sql",
	)
}

func _000004_add_updated_created_atUpSql() (*asset, error) {
	bytes, err := _000004_add_updated_created_atUpSqlBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "000004_add_updated_created_at.up.sql", size: 498, mode: os.FileMode(436), modTime: time.Unix(1589389939, 0)}
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
	"000001_create_org_user_tables.down.sql":      _000001_create_org_user_tablesDownSql,
	"000001_create_org_user_tables.up.sql":        _000001_create_org_user_tablesUpSql,
	"000002_add_unique_constraint_email.down.sql": _000002_add_unique_constraint_emailDownSql,
	"000002_add_unique_constraint_email.up.sql":   _000002_add_unique_constraint_emailUpSql,
	"000003_add_profile_picture.down.sql":         _000003_add_profile_pictureDownSql,
	"000003_add_profile_picture.up.sql":           _000003_add_profile_pictureUpSql,
	"000004_add_updated_created_at.down.sql":      _000004_add_updated_created_atDownSql,
	"000004_add_updated_created_at.up.sql":        _000004_add_updated_created_atUpSql,
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
	"000001_create_org_user_tables.down.sql":      &bintree{_000001_create_org_user_tablesDownSql, map[string]*bintree{}},
	"000001_create_org_user_tables.up.sql":        &bintree{_000001_create_org_user_tablesUpSql, map[string]*bintree{}},
	"000002_add_unique_constraint_email.down.sql": &bintree{_000002_add_unique_constraint_emailDownSql, map[string]*bintree{}},
	"000002_add_unique_constraint_email.up.sql":   &bintree{_000002_add_unique_constraint_emailUpSql, map[string]*bintree{}},
	"000003_add_profile_picture.down.sql":         &bintree{_000003_add_profile_pictureDownSql, map[string]*bintree{}},
	"000003_add_profile_picture.up.sql":           &bintree{_000003_add_profile_pictureUpSql, map[string]*bintree{}},
	"000004_add_updated_created_at.down.sql":      &bintree{_000004_add_updated_created_atDownSql, map[string]*bintree{}},
	"000004_add_updated_created_at.up.sql":        &bintree{_000004_add_updated_created_atUpSql, map[string]*bintree{}},
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
