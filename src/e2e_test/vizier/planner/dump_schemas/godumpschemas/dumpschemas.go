//go:build cgo

/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package godumpschemas

// // The following is live code even though it is commented out.
// // If you delete it, this will break.
// #include <stdlib.h>
// #include "src/e2e_test/vizier/planner/dump_schemas/dump_schemas.h"
import "C"
import (
	"errors"
	"fmt"
	"unsafe"

	"github.com/gogo/protobuf/proto"

	"px.dev/pixie/src/table_store/schemapb"
)

// DumpSchemas dumps all the table schemas from stirling.
func DumpSchemas() (*schemapb.Schema, error) {

	var resLen C.int
	res := C.DumpSchemas(&resLen)
	defer C.SchemaStrFree(res)
	schemaBytes := C.GoBytes(unsafe.Pointer(res), resLen)
	if resLen == 0 {
		return nil, errors.New("no result returned")
	}

	schema := &schemapb.Schema{}
	if err := proto.Unmarshal(schemaBytes, schema); err != nil {
		return nil, fmt.Errorf("error: '%s'; string: '%s'", err, string(schemaBytes))
	}
	return schema, nil
}
