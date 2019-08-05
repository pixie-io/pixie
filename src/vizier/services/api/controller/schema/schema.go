/**
  GraphQL Schema setup file.
*/
//go:generate go-bindata -ignore=\.go -ignore=\.ts -ignore=\.sh -ignore=\.bazel -pkg=schema -o=bindata.gen.go ./...

package schema

import "bytes"

// MustLoadSchema reads all the bindata .graphql schema files and concats them together into
// one string.
func MustLoadSchema() string {
	buf := bytes.Buffer{}
	for _, name := range AssetNames() {
		b := MustAsset(name)
		buf.Write(b)

		// Add a newline if the file does not end in a newline.
		if len(b) > 0 && b[len(b)-1] != '\n' {
			buf.WriteByte('\n')
		}
	}

	return buf.String()
}
