// Code generated for package funcs by go-bindata DO NOT EDIT. (@generated)
// sources:
// bazel-out/k8-fastbuild/bin/src/vizier/funcs/data/udf.pb
package funcs

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

var _srcVizierFuncsDataUdfPb = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x84\x96\xcf\x6f\xdb\x36\x14\xc7\x61\x5a\xd2\xb2\x97\xd4\x51\x94\xba\x4b\x9d\x6d\x10\x8a\x9d\x7d\x1a\x76\x2f\x86\xa0\xb7\xed\x90\xdd\x05\x5a\x62\x6d\x62\x14\xc5\x8a\x54\xd7\x9e\xf7\x8f\x0f\x7c\x94\x6c\x8a\x62\xd6\x5b\xde\xe7\xfb\x7e\x89\x7c\xcf\x0c\xdc\xc2\x95\x12\xfb\xba\x1b\xa4\xd9\xad\x56\x25\x99\x03\x12\x82\x24\x04\x69\x49\xe0\x15\x64\x4a\xec\x5b\xfa\xc5\x05\x78\x66\x52\x26\xb0\x81\xef\xac\xc9\xa8\xb4\x05\xe6\x36\x09\x6c\xeb\x3f\x86\x73\x39\xcb\xc6\x47\xf5\x1e\x6e\x94\xd8\x7f\x1a\xa8\x34\x5c\x30\x6d\x7d\xd2\x25\x4c\xca\x74\x0c\xd4\x43\x6b\xcb\xae\x7c\xf3\x92\x16\xcd\xa4\x4c\x8a\x0d\x9a\xb4\x69\x76\x84\x90\x92\xcc\xec\x40\x4f\x48\x60\x07\x7a\x46\xca\xac\x78\x03\x1b\x6b\x2b\xd5\x77\x5f\x9e\x3e\x0d\x54\xa0\xdf\xaa\xb8\xc6\x8f\xa5\x9a\x37\xe7\x22\x07\x2e\x67\x45\x9d\x9d\xcd\x6d\x4c\xea\xdb\x59\x99\x15\xf7\x70\x8d\x17\x21\x0d\xe5\x52\xef\x48\x9a\x96\xab\xe2\x0e\xbe\x57\x62\xdf\xf0\xcf\xbc\x61\x63\xde\x00\xd9\x7e\xe7\x08\x3f\x29\x40\xd6\x2b\xc7\x9b\x66\xae\x7f\x7b\x8a\x01\x21\x21\x21\x0b\x1f\xb2\xf4\x49\x42\xb2\x5e\x87\x24\x59\x44\x25\x8b\x28\xfc\xda\x5b\x3c\xcf\x8f\x5c\x36\x08\xc8\x78\xf0\xc7\x9e\x51\xc3\xfa\xbf\x4e\x54\x8e\x2d\x44\x38\xa6\x8c\x70\x4c\xfc\x08\xf7\x73\xfe\xe4\x7d\xcf\x4b\x22\x66\x7c\x49\x1c\xfb\xb5\x57\x76\xea\xb4\x91\xb4\x65\x65\x5a\xe4\x78\xea\x82\xc9\xa3\x39\xe1\x36\x8d\xb7\x2a\x98\xd6\x5e\xfb\x21\xc4\x4a\x21\xc4\x0a\x0f\x90\x7b\xd0\xef\x3a\xaa\x60\xa2\xa8\x82\xd9\xb6\xf0\xca\x2a\xdd\x91\xd7\x54\xbc\xb7\xa7\x8c\x63\xb0\xc4\x58\xe1\xb5\x8f\xff\xe8\xf0\x17\x25\x46\x9d\xef\xcd\x85\xfe\xd9\x8f\x89\x17\x14\xf3\xba\xd1\x6c\xbb\x66\x10\xdd\x6c\xa6\xcf\x28\x5b\xa0\x6c\xe9\x95\x65\xe7\xe3\x6d\x07\x61\xb8\x12\x5f\xc7\x6c\x0b\x68\xa7\x3f\x84\xb8\x25\x0b\xe8\xf6\xc4\x96\x91\xec\x48\x0d\xc3\x9f\x97\x80\x5c\xb2\xc9\xce\x3c\x79\xeb\xb4\x80\x24\x02\x49\xcc\x93\x44\x3d\x93\x08\x4c\x62\x9e\x49\xcc\x13\x6f\xdc\xed\x98\x12\x43\xfd\x37\x92\x74\x9c\x0e\x24\xd5\x47\xd1\x51\xf3\xdb\xaf\xa8\x24\xe3\xf2\x38\x85\xcb\x89\x93\xe2\x47\xd8\x5a\xde\x35\x15\x6f\x2a\xd3\x55\xf6\x2f\x3b\xef\x76\xc0\xd3\xe2\x67\xf8\x61\xa6\x6a\xd6\x7f\xe6\x35\xab\x78\xe3\xf4\x12\xde\x46\xf5\xff\xc9\x60\x68\x6f\x2a\xc3\x9d\x9e\x79\xf5\x6d\xcc\xd4\xc1\x32\xff\xa4\x86\x1d\xbc\x83\xc7\x17\x3c\x2e\x3d\x44\xb2\x7c\xb3\x0b\x6d\xa8\x19\xb4\x8b\x77\xe7\xdc\x77\x83\x74\x6f\x4a\x5a\xfc\x02\x3f\xd9\x27\xe9\xdc\x4b\xbc\xee\xdc\xeb\xa5\x2f\xd8\xe2\x1e\xe9\xe1\xa0\x4d\xcf\xe5\x71\xb7\x4e\x89\x2d\xe1\x6e\x5c\x0f\x07\xd3\xd3\xda\xcc\x86\xdf\x83\x97\x71\xf5\x60\x16\xf1\xf4\x36\xc2\x83\xb1\xf0\x2c\x56\x28\x73\x5b\x0b\x4a\xec\x4d\x27\xba\x7f\x58\xef\x9a\x9f\xd0\xa0\xd4\x84\xdc\x3f\x09\xa6\xe7\xad\xb3\xdf\xc0\xad\x12\xfb\x41\xb9\x53\xb2\xef\xe9\x6e\xb5\x2e\x49\xf1\x16\x0a\x8f\xd7\x6d\x23\xb8\x64\x56\x9a\x86\xe6\x2c\xb9\x57\x93\xf5\x95\x0b\x4d\x8b\x47\x78\xed\xe9\xf6\x60\xb5\xa2\xf5\x18\xbc\xc5\x49\x9f\x44\x35\x95\x7b\x80\x3b\x1f\x8f\x43\x66\x03\x76\xf8\x0a\xf8\x8a\xbb\x42\xab\xb9\xc1\x98\x34\xff\xea\x96\x7d\xce\x07\x60\x8d\xeb\xe8\xd7\x1c\xef\xd7\x2a\xbb\x7f\x57\xb0\xf9\xc0\xcc\xfb\x23\x93\xe6\x19\x47\xed\x21\x7b\xd7\xc1\x0d\x5c\x51\x8b\x2a\xde\xe4\x6b\xb8\x82\xc4\x1e\x57\x4e\x2c\x9f\x1e\xa2\x3c\x85\x0d\x00\x57\x15\x6d\x9a\x9e\x69\x9d\xa7\x70\x0b\xd7\x2e\xca\x0e\x2d\x73\xa0\xc6\x47\x0d\x47\x3c\xcf\x60\x0b\x77\x82\x6a\x53\x9d\x18\xed\xcd\x81\x51\x53\x49\x9d\x93\xdd\x33\xc0\x07\x66\x9e\xeb\x13\x6b\xa9\x6d\xe0\x77\x9b\xda\xd0\x83\x70\x1f\x31\x66\xea\xc4\xd0\xca\x25\x30\x5f\xd5\x1c\x34\x4c\xd7\x79\xfa\x5f\x00\x00\x00\xff\xff\xc7\x83\x6d\x1f\xa7\x0a\x00\x00")

func srcVizierFuncsDataUdfPbBytes() ([]byte, error) {
	return bindataRead(
		_srcVizierFuncsDataUdfPb,
		"src/vizier/funcs/data/udf.pb",
	)
}

func srcVizierFuncsDataUdfPb() (*asset, error) {
	bytes, err := srcVizierFuncsDataUdfPbBytes()
	if err != nil {
		return nil, err
	}

	info := bindataFileInfo{name: "src/vizier/funcs/data/udf.pb", size: 0, mode: os.FileMode(0), modTime: time.Unix(0, 0)}
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
	"src/vizier/funcs/data/udf.pb": srcVizierFuncsDataUdfPb,
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
	"src": &bintree{nil, map[string]*bintree{
		"vizier": &bintree{nil, map[string]*bintree{
			"funcs": &bintree{nil, map[string]*bintree{
				"data": &bintree{nil, map[string]*bintree{
					"udf.pb": &bintree{srcVizierFuncsDataUdfPb, map[string]*bintree{}},
				}},
			}},
		}},
	}},
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
