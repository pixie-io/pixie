package utils

import (
	"database/sql/driver"
	"errors"

	"github.com/lib/pq"
	"pixielabs.ai/pixielabs/src/shared/artifacts/versionspb"
)

// Enum values for artifact type.
const (
	ATUnknown                ArtifactTypeDB = "UNKNOWN"
	ATLinuxAMD64             ArtifactTypeDB = "LINUX_AMD64"
	ATDarwinAMD64            ArtifactTypeDB = "DARWIN_AMD64"
	ATContainerSetLinuxAMD64 ArtifactTypeDB = "CONTAINER_SET_LINUX_AMD64"
)

// ArtifactTypeDB is the DB representation of the proto ArtifactType.
type ArtifactTypeDB string

// Scan reads values from DB and converts to native type.
func (s *ArtifactTypeDB) Scan(value interface{}) error {
	asBytes, ok := value.([]byte)
	if !ok {
		return errors.New("Scan source is not []byte")
	}
	*s = ArtifactTypeDB(string(asBytes))
	return nil
}

// Value coverts native type to DB.
func (s ArtifactTypeDB) Value() (driver.Value, error) {
	return string(s), nil
}

// ToArtifactTypeDB converts proto enum to DB enum.
func ToArtifactTypeDB(a versionspb.ArtifactType) ArtifactTypeDB {
	switch a {
	case versionspb.AT_LINUX_AMD64:
		return ATLinuxAMD64
	case versionspb.AT_DARWIN_AMD64:
		return ATDarwinAMD64
	case versionspb.AT_CONTAINER_SET_LINUX_AMD64:
		return ATContainerSetLinuxAMD64
	default:
		return ATUnknown
	}
}

// ToProtoArtifactType converts DB enum to proto enum.
func ToProtoArtifactType(a ArtifactTypeDB) versionspb.ArtifactType {
	switch a {
	case ATLinuxAMD64:
		return versionspb.AT_LINUX_AMD64
	case ATDarwinAMD64:
		return versionspb.AT_DARWIN_AMD64
	case ATContainerSetLinuxAMD64:
		return versionspb.AT_CONTAINER_SET_LINUX_AMD64
	default:
		return versionspb.AT_UNKNOWN
	}
}

// ToProtoArtifactTypeArray converts db enum array to array of proto types.
func ToProtoArtifactTypeArray(a pq.StringArray) []versionspb.ArtifactType {
	res := make([]versionspb.ArtifactType, len(a))
	for i, val := range a {
		res[i] = ToProtoArtifactType(ArtifactTypeDB(val))
	}
	return res
}

// ToArtifactArray coverts ArtifactType to db native pq.StringArray enum.
func ToArtifactArray(artifactType []versionspb.ArtifactType) pq.StringArray {
	res := make([]string, len(artifactType))
	for i, a := range artifactType {
		res[i] = string(ToArtifactTypeDB(a))
	}
	return res
}
