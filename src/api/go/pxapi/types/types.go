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

package types

import (
	"encoding/binary"
	"fmt"
	"time"

	"github.com/gofrs/uuid"

	"px.dev/pixie/src/api/proto/vizierpb"
)

// DataType referes to the underlying Pixie datatype.
type DataType = vizierpb.DataType

// SemanticType stores semantic information about the underlying data. For example, the value represents a throughput,
// or K8s entity, etc.
type SemanticType = vizierpb.SemanticType

// Datum is a base type use to wrap all underlying Pixie types.
type Datum interface {
	// String returns the string representation, regardless of the type.
	String() string
	// Type returns the Pixie data type.
	Type() DataType
	// TODO(zasgar): Enabled the formatted value interface. The goal for this function
	// will be to provide the canonical value with units.
	// FormattedValue() FormattedValue
	// SemanticType returns the Pixie semantic type.
	SemanticType() SemanticType
}

// ValueType is the shared structure of values.
type ValueType struct {
	// ColInfo is a pointer to the column schema. We maintain this as a pointer here
	// since the col schema is duplicated from the table structure.
	ColInfo *ColSchema
}

// Type returns the Pixie data type.
func (v ValueType) Type() DataType {
	return v.ColInfo.Type
}

// SemanticType returns the Pixie semantic type.
func (v ValueType) SemanticType() SemanticType {
	return v.ColInfo.SemanticType
}

/****************
 * Boolean
 *****************/

// BooleanValue is the concrete type that holds an boolean value.
type BooleanValue struct {
	ValueType
	val bool
}

// NewBooleanValue constructs a BooleanValue.
func NewBooleanValue(schema *ColSchema) *BooleanValue {
	return &BooleanValue{
		ValueType: ValueType{schema},
	}
}

// String returns the string representation.
func (v BooleanValue) String() string {
	return fmt.Sprintf("%t", v.val)
}

// Value returns the data as a bool
func (v BooleanValue) Value() bool {
	return v.val
}

// ScanBool scans bool value and write it to the internal data.
func (v *BooleanValue) ScanBool(data bool) {
	v.val = data
}

/****************
 * Int64
 *****************/

// Int64Value is the concrete type that holds an int64 value.
type Int64Value struct {
	ValueType
	val int64
}

// NewInt64Value constructs an Int64Value.
func NewInt64Value(schema *ColSchema) *Int64Value {
	return &Int64Value{
		ValueType: ValueType{schema},
	}
}

// String returns the string representation.
func (v Int64Value) String() string {
	return fmt.Sprintf("%d", v.val)
}

// Value returns the data as an int64.
func (v Int64Value) Value() int64 {
	return v.val
}

// ScanInt64 writes the int64 to the internal data structure.
func (v *Int64Value) ScanInt64(data int64) {
	v.val = data
}

/****************
 * String
 *****************/

// StringValue is the concrete type that holds an string value.
type StringValue struct {
	ValueType
	val string
}

// NewStringValue constructs a StringValue.
func NewStringValue(schema *ColSchema) *StringValue {
	return &StringValue{
		ValueType: ValueType{schema},
	}
}

// String returns the string representation.
func (v StringValue) String() string {
	return v.val
}

// Value returns the string.
func (v StringValue) Value() string {
	return v.val
}

// ScanString store the passed in string.
func (v *StringValue) ScanString(data string) {
	v.val = data
}

/****************
 * Float64
 *****************/

// Float64Value is the concrete type that holds an float64 value.
type Float64Value struct {
	ValueType
	val float64
}

// NewFloat64Value constructs a Float64Value.
func NewFloat64Value(schema *ColSchema) *Float64Value {
	return &Float64Value{
		ValueType: ValueType{schema},
	}
}

// String returns the string representation.
func (v Float64Value) String() string {
	return fmt.Sprintf("%f", v.val)
}

// Value returns the data as a float64.
func (v Float64Value) Value() float64 {
	return v.val
}

// ScanFloat64 stores the float64 data.
func (v *Float64Value) ScanFloat64(data float64) {
	v.val = data
}

/****************
 * Time64NS
 *****************/

// Time64NSValue is the concrete type that holds an time value.
type Time64NSValue struct {
	ValueType
	val time.Time
}

// NewTime64NSValue constructs a NewTime64NSValue.
func NewTime64NSValue(schema *ColSchema) *Time64NSValue {
	return &Time64NSValue{
		ValueType: ValueType{schema},
	}
}

// String returns the string representation.
func (v Time64NSValue) String() string {
	return fmt.Sprintf("%v", v.val)
}

// Value returns the data as a time.
func (v Time64NSValue) Value() time.Time {
	return v.val
}

// ScanInt64 stores the int64 data.
func (v *Time64NSValue) ScanInt64(data int64) {
	v.val = time.Unix(0, data)
}

/****************
 * UINT128
 *****************/

// UInt128Value is the concrete type that holds an time value.
type UInt128Value struct {
	ValueType
	b []byte
}

// NewUint128Value constructs a UInt128Value.
func NewUint128Value(schema *ColSchema) *UInt128Value {
	return &UInt128Value{
		ValueType: ValueType{schema},
		b:         make([]byte, 16),
	}
}

// String returns the string representation.
func (v UInt128Value) String() string {
	s := uuid.FromBytesOrNil(v.b).String()
	return fmt.Sprintf("%v", s)
}

// Value returns the data as bytes.
func (v UInt128Value) Value() []byte {
	return v.b
}

// ScanUInt128 stores the passed in proto UInt128.
func (v *UInt128Value) ScanUInt128(data *vizierpb.UInt128) {
	b2 := v.b[8:]
	binary.BigEndian.PutUint64(v.b, data.High)
	binary.BigEndian.PutUint64(b2, data.Low)
}
