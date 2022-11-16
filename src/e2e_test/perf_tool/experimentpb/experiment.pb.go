// Code generated by protoc-gen-gogo. DO NOT EDIT.
// source: src/e2e_test/perf_tool/experimentpb/experiment.proto

package experimentpb

import (
	fmt "fmt"
	_ "github.com/gogo/protobuf/gogoproto"
	proto "github.com/gogo/protobuf/proto"
	io "io"
	math "math"
	math_bits "math/bits"
	reflect "reflect"
	strings "strings"
)

// Reference imports to suppress errors if they are not otherwise used.
var _ = proto.Marshal
var _ = fmt.Errorf
var _ = math.Inf

// This is a compile-time assertion to ensure that this generated file
// is compatible with the proto package it is being compiled against.
// A compilation error at this line likely means your copy of the
// proto package needs to be updated.
const _ = proto.GoGoProtoPackageIsVersion3 // please upgrade the proto package

type ExperimentSpec struct {
	VersionSpec   *VersionSpec    `protobuf:"bytes,1,opt,name=version_spec,json=versionSpec,proto3" json:"version_spec,omitempty"`
	VizierSpec    *WorkloadSpec   `protobuf:"bytes,2,opt,name=vizier_spec,json=vizierSpec,proto3" json:"vizier_spec,omitempty"`
	WorkloadSpecs []*WorkloadSpec `protobuf:"bytes,3,rep,name=workload_specs,json=workloadSpecs,proto3" json:"workload_specs,omitempty"`
	MetricSpecs   []*MetricSpec   `protobuf:"bytes,4,rep,name=metric_specs,json=metricSpecs,proto3" json:"metric_specs,omitempty"`
	ClusterSpec   *ClusterSpec    `protobuf:"bytes,5,opt,name=cluster_spec,json=clusterSpec,proto3" json:"cluster_spec,omitempty"`
}

func (m *ExperimentSpec) Reset()      { *m = ExperimentSpec{} }
func (*ExperimentSpec) ProtoMessage() {}
func (*ExperimentSpec) Descriptor() ([]byte, []int) {
	return fileDescriptor_96d7e52dda1e6fe3, []int{0}
}
func (m *ExperimentSpec) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *ExperimentSpec) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_ExperimentSpec.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *ExperimentSpec) XXX_Merge(src proto.Message) {
	xxx_messageInfo_ExperimentSpec.Merge(m, src)
}
func (m *ExperimentSpec) XXX_Size() int {
	return m.Size()
}
func (m *ExperimentSpec) XXX_DiscardUnknown() {
	xxx_messageInfo_ExperimentSpec.DiscardUnknown(m)
}

var xxx_messageInfo_ExperimentSpec proto.InternalMessageInfo

func (m *ExperimentSpec) GetVersionSpec() *VersionSpec {
	if m != nil {
		return m.VersionSpec
	}
	return nil
}

func (m *ExperimentSpec) GetVizierSpec() *WorkloadSpec {
	if m != nil {
		return m.VizierSpec
	}
	return nil
}

func (m *ExperimentSpec) GetWorkloadSpecs() []*WorkloadSpec {
	if m != nil {
		return m.WorkloadSpecs
	}
	return nil
}

func (m *ExperimentSpec) GetMetricSpecs() []*MetricSpec {
	if m != nil {
		return m.MetricSpecs
	}
	return nil
}

func (m *ExperimentSpec) GetClusterSpec() *ClusterSpec {
	if m != nil {
		return m.ClusterSpec
	}
	return nil
}

type VersionSpec struct {
}

func (m *VersionSpec) Reset()      { *m = VersionSpec{} }
func (*VersionSpec) ProtoMessage() {}
func (*VersionSpec) Descriptor() ([]byte, []int) {
	return fileDescriptor_96d7e52dda1e6fe3, []int{1}
}
func (m *VersionSpec) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *VersionSpec) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_VersionSpec.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *VersionSpec) XXX_Merge(src proto.Message) {
	xxx_messageInfo_VersionSpec.Merge(m, src)
}
func (m *VersionSpec) XXX_Size() int {
	return m.Size()
}
func (m *VersionSpec) XXX_DiscardUnknown() {
	xxx_messageInfo_VersionSpec.DiscardUnknown(m)
}

var xxx_messageInfo_VersionSpec proto.InternalMessageInfo

type WorkloadSpec struct {
}

func (m *WorkloadSpec) Reset()      { *m = WorkloadSpec{} }
func (*WorkloadSpec) ProtoMessage() {}
func (*WorkloadSpec) Descriptor() ([]byte, []int) {
	return fileDescriptor_96d7e52dda1e6fe3, []int{2}
}
func (m *WorkloadSpec) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *WorkloadSpec) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_WorkloadSpec.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *WorkloadSpec) XXX_Merge(src proto.Message) {
	xxx_messageInfo_WorkloadSpec.Merge(m, src)
}
func (m *WorkloadSpec) XXX_Size() int {
	return m.Size()
}
func (m *WorkloadSpec) XXX_DiscardUnknown() {
	xxx_messageInfo_WorkloadSpec.DiscardUnknown(m)
}

var xxx_messageInfo_WorkloadSpec proto.InternalMessageInfo

type MetricSpec struct {
}

func (m *MetricSpec) Reset()      { *m = MetricSpec{} }
func (*MetricSpec) ProtoMessage() {}
func (*MetricSpec) Descriptor() ([]byte, []int) {
	return fileDescriptor_96d7e52dda1e6fe3, []int{3}
}
func (m *MetricSpec) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *MetricSpec) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_MetricSpec.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *MetricSpec) XXX_Merge(src proto.Message) {
	xxx_messageInfo_MetricSpec.Merge(m, src)
}
func (m *MetricSpec) XXX_Size() int {
	return m.Size()
}
func (m *MetricSpec) XXX_DiscardUnknown() {
	xxx_messageInfo_MetricSpec.DiscardUnknown(m)
}

var xxx_messageInfo_MetricSpec proto.InternalMessageInfo

type ClusterSpec struct {
}

func (m *ClusterSpec) Reset()      { *m = ClusterSpec{} }
func (*ClusterSpec) ProtoMessage() {}
func (*ClusterSpec) Descriptor() ([]byte, []int) {
	return fileDescriptor_96d7e52dda1e6fe3, []int{4}
}
func (m *ClusterSpec) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *ClusterSpec) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_ClusterSpec.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *ClusterSpec) XXX_Merge(src proto.Message) {
	xxx_messageInfo_ClusterSpec.Merge(m, src)
}
func (m *ClusterSpec) XXX_Size() int {
	return m.Size()
}
func (m *ClusterSpec) XXX_DiscardUnknown() {
	xxx_messageInfo_ClusterSpec.DiscardUnknown(m)
}

var xxx_messageInfo_ClusterSpec proto.InternalMessageInfo

type Empty struct {
}

func (m *Empty) Reset()      { *m = Empty{} }
func (*Empty) ProtoMessage() {}
func (*Empty) Descriptor() ([]byte, []int) {
	return fileDescriptor_96d7e52dda1e6fe3, []int{5}
}
func (m *Empty) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *Empty) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_Empty.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *Empty) XXX_Merge(src proto.Message) {
	xxx_messageInfo_Empty.Merge(m, src)
}
func (m *Empty) XXX_Size() int {
	return m.Size()
}
func (m *Empty) XXX_DiscardUnknown() {
	xxx_messageInfo_Empty.DiscardUnknown(m)
}

var xxx_messageInfo_Empty proto.InternalMessageInfo

func init() {
	proto.RegisterType((*ExperimentSpec)(nil), "px.perf_tool.ExperimentSpec")
	proto.RegisterType((*VersionSpec)(nil), "px.perf_tool.VersionSpec")
	proto.RegisterType((*WorkloadSpec)(nil), "px.perf_tool.WorkloadSpec")
	proto.RegisterType((*MetricSpec)(nil), "px.perf_tool.MetricSpec")
	proto.RegisterType((*ClusterSpec)(nil), "px.perf_tool.ClusterSpec")
	proto.RegisterType((*Empty)(nil), "px.perf_tool.Empty")
}

func init() {
	proto.RegisterFile("src/e2e_test/perf_tool/experimentpb/experiment.proto", fileDescriptor_96d7e52dda1e6fe3)
}

var fileDescriptor_96d7e52dda1e6fe3 = []byte{
	// 348 bytes of a gzipped FileDescriptorProto
	0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x84, 0x92, 0x3f, 0x4f, 0x02, 0x31,
	0x18, 0xc6, 0xaf, 0x20, 0x9a, 0xf4, 0x0e, 0x86, 0x9b, 0x90, 0xe1, 0x0d, 0xb9, 0xc9, 0xc5, 0xbb,
	0x04, 0xdd, 0x70, 0x11, 0xc3, 0xe8, 0xa2, 0x89, 0x26, 0x2e, 0xc4, 0xab, 0x05, 0x2f, 0x72, 0xb4,
	0x69, 0xcb, 0x1f, 0x9d, 0xfc, 0x08, 0x7e, 0x0c, 0x3f, 0x8a, 0x6e, 0x8c, 0x8c, 0x52, 0x16, 0x47,
	0x3e, 0x82, 0xa1, 0x45, 0x28, 0x89, 0x89, 0xdb, 0xfb, 0xa4, 0xcf, 0xef, 0x79, 0xdf, 0xb7, 0x2d,
	0x3e, 0x95, 0x82, 0x24, 0xb4, 0x41, 0x3b, 0x8a, 0x4a, 0x95, 0x70, 0x2a, 0xba, 0x1d, 0xc5, 0x58,
	0x3f, 0xa1, 0x13, 0x4e, 0x45, 0x96, 0xd3, 0x81, 0xe2, 0xa9, 0x23, 0x62, 0x2e, 0x98, 0x62, 0x61,
	0xc0, 0x27, 0xf1, 0xc6, 0x5b, 0x3b, 0xee, 0x65, 0xea, 0x71, 0x98, 0xc6, 0x84, 0xe5, 0x49, 0x8f,
	0xf5, 0x58, 0x62, 0x4c, 0xe9, 0xb0, 0x6b, 0x94, 0x11, 0xa6, 0xb2, 0x70, 0xf4, 0x59, 0xc0, 0x95,
	0xf6, 0x26, 0xf1, 0x9a, 0x53, 0x12, 0x9e, 0xe1, 0x60, 0x44, 0x85, 0xcc, 0xd8, 0xa0, 0x23, 0x39,
	0x25, 0x55, 0x54, 0x47, 0x47, 0x7e, 0xe3, 0x30, 0x76, 0xdb, 0xc4, 0x37, 0xd6, 0xb1, 0x02, 0xae,
	0xfc, 0xd1, 0x56, 0x84, 0x4d, 0xec, 0x8f, 0xb2, 0x97, 0x8c, 0x0a, 0x0b, 0x17, 0x0c, 0x5c, 0xdb,
	0x85, 0x6f, 0x99, 0x78, 0xea, 0xb3, 0xfb, 0x07, 0x43, 0x63, 0x6b, 0x37, 0xf0, 0x39, 0xae, 0x8c,
	0xd7, 0x67, 0x06, 0x97, 0xd5, 0x62, 0xbd, 0xf8, 0x0f, 0x5f, 0x1e, 0x3b, 0x4a, 0x86, 0x4d, 0x1c,
	0xe4, 0x54, 0x89, 0x8c, 0xac, 0x03, 0xf6, 0x4c, 0x40, 0x75, 0x37, 0xe0, 0xd2, 0x38, 0xec, 0xf0,
	0xf9, 0xa6, 0x96, 0xab, 0xd5, 0x49, 0x7f, 0x28, 0xd5, 0xef, 0xf4, 0xa5, 0xbf, 0x56, 0xbf, 0xb0,
	0x0e, 0x4b, 0x93, 0xad, 0x88, 0xca, 0xd8, 0x77, 0xae, 0x25, 0xaa, 0xe0, 0xc0, 0x1d, 0x34, 0x0a,
	0x30, 0xde, 0xf6, 0x5d, 0x99, 0x9d, 0xa0, 0xe8, 0x00, 0x97, 0xda, 0x39, 0x57, 0xcf, 0xad, 0xd6,
	0x74, 0x0e, 0xde, 0x6c, 0x0e, 0xde, 0x72, 0x0e, 0xe8, 0x55, 0x03, 0x7a, 0xd7, 0x80, 0x3e, 0x34,
	0xa0, 0xa9, 0x06, 0xf4, 0xa5, 0x01, 0x7d, 0x6b, 0xf0, 0x96, 0x1a, 0xd0, 0xdb, 0x02, 0xbc, 0xe9,
	0x02, 0xbc, 0xd9, 0x02, 0xbc, 0xbb, 0xc0, 0xfd, 0x22, 0xe9, 0xbe, 0x79, 0xdb, 0x93, 0x9f, 0x00,
	0x00, 0x00, 0xff, 0xff, 0x8d, 0xf9, 0x3b, 0x40, 0x50, 0x02, 0x00, 0x00,
}

func (this *ExperimentSpec) Equal(that interface{}) bool {
	if that == nil {
		return this == nil
	}

	that1, ok := that.(*ExperimentSpec)
	if !ok {
		that2, ok := that.(ExperimentSpec)
		if ok {
			that1 = &that2
		} else {
			return false
		}
	}
	if that1 == nil {
		return this == nil
	} else if this == nil {
		return false
	}
	if !this.VersionSpec.Equal(that1.VersionSpec) {
		return false
	}
	if !this.VizierSpec.Equal(that1.VizierSpec) {
		return false
	}
	if len(this.WorkloadSpecs) != len(that1.WorkloadSpecs) {
		return false
	}
	for i := range this.WorkloadSpecs {
		if !this.WorkloadSpecs[i].Equal(that1.WorkloadSpecs[i]) {
			return false
		}
	}
	if len(this.MetricSpecs) != len(that1.MetricSpecs) {
		return false
	}
	for i := range this.MetricSpecs {
		if !this.MetricSpecs[i].Equal(that1.MetricSpecs[i]) {
			return false
		}
	}
	if !this.ClusterSpec.Equal(that1.ClusterSpec) {
		return false
	}
	return true
}
func (this *VersionSpec) Equal(that interface{}) bool {
	if that == nil {
		return this == nil
	}

	that1, ok := that.(*VersionSpec)
	if !ok {
		that2, ok := that.(VersionSpec)
		if ok {
			that1 = &that2
		} else {
			return false
		}
	}
	if that1 == nil {
		return this == nil
	} else if this == nil {
		return false
	}
	return true
}
func (this *WorkloadSpec) Equal(that interface{}) bool {
	if that == nil {
		return this == nil
	}

	that1, ok := that.(*WorkloadSpec)
	if !ok {
		that2, ok := that.(WorkloadSpec)
		if ok {
			that1 = &that2
		} else {
			return false
		}
	}
	if that1 == nil {
		return this == nil
	} else if this == nil {
		return false
	}
	return true
}
func (this *MetricSpec) Equal(that interface{}) bool {
	if that == nil {
		return this == nil
	}

	that1, ok := that.(*MetricSpec)
	if !ok {
		that2, ok := that.(MetricSpec)
		if ok {
			that1 = &that2
		} else {
			return false
		}
	}
	if that1 == nil {
		return this == nil
	} else if this == nil {
		return false
	}
	return true
}
func (this *ClusterSpec) Equal(that interface{}) bool {
	if that == nil {
		return this == nil
	}

	that1, ok := that.(*ClusterSpec)
	if !ok {
		that2, ok := that.(ClusterSpec)
		if ok {
			that1 = &that2
		} else {
			return false
		}
	}
	if that1 == nil {
		return this == nil
	} else if this == nil {
		return false
	}
	return true
}
func (this *Empty) Equal(that interface{}) bool {
	if that == nil {
		return this == nil
	}

	that1, ok := that.(*Empty)
	if !ok {
		that2, ok := that.(Empty)
		if ok {
			that1 = &that2
		} else {
			return false
		}
	}
	if that1 == nil {
		return this == nil
	} else if this == nil {
		return false
	}
	return true
}
func (this *ExperimentSpec) GoString() string {
	if this == nil {
		return "nil"
	}
	s := make([]string, 0, 9)
	s = append(s, "&experimentpb.ExperimentSpec{")
	if this.VersionSpec != nil {
		s = append(s, "VersionSpec: "+fmt.Sprintf("%#v", this.VersionSpec)+",\n")
	}
	if this.VizierSpec != nil {
		s = append(s, "VizierSpec: "+fmt.Sprintf("%#v", this.VizierSpec)+",\n")
	}
	if this.WorkloadSpecs != nil {
		s = append(s, "WorkloadSpecs: "+fmt.Sprintf("%#v", this.WorkloadSpecs)+",\n")
	}
	if this.MetricSpecs != nil {
		s = append(s, "MetricSpecs: "+fmt.Sprintf("%#v", this.MetricSpecs)+",\n")
	}
	if this.ClusterSpec != nil {
		s = append(s, "ClusterSpec: "+fmt.Sprintf("%#v", this.ClusterSpec)+",\n")
	}
	s = append(s, "}")
	return strings.Join(s, "")
}
func (this *VersionSpec) GoString() string {
	if this == nil {
		return "nil"
	}
	s := make([]string, 0, 4)
	s = append(s, "&experimentpb.VersionSpec{")
	s = append(s, "}")
	return strings.Join(s, "")
}
func (this *WorkloadSpec) GoString() string {
	if this == nil {
		return "nil"
	}
	s := make([]string, 0, 4)
	s = append(s, "&experimentpb.WorkloadSpec{")
	s = append(s, "}")
	return strings.Join(s, "")
}
func (this *MetricSpec) GoString() string {
	if this == nil {
		return "nil"
	}
	s := make([]string, 0, 4)
	s = append(s, "&experimentpb.MetricSpec{")
	s = append(s, "}")
	return strings.Join(s, "")
}
func (this *ClusterSpec) GoString() string {
	if this == nil {
		return "nil"
	}
	s := make([]string, 0, 4)
	s = append(s, "&experimentpb.ClusterSpec{")
	s = append(s, "}")
	return strings.Join(s, "")
}
func (this *Empty) GoString() string {
	if this == nil {
		return "nil"
	}
	s := make([]string, 0, 4)
	s = append(s, "&experimentpb.Empty{")
	s = append(s, "}")
	return strings.Join(s, "")
}
func valueToGoStringExperiment(v interface{}, typ string) string {
	rv := reflect.ValueOf(v)
	if rv.IsNil() {
		return "nil"
	}
	pv := reflect.Indirect(rv).Interface()
	return fmt.Sprintf("func(v %v) *%v { return &v } ( %#v )", typ, typ, pv)
}
func (m *ExperimentSpec) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *ExperimentSpec) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *ExperimentSpec) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	if m.ClusterSpec != nil {
		{
			size, err := m.ClusterSpec.MarshalToSizedBuffer(dAtA[:i])
			if err != nil {
				return 0, err
			}
			i -= size
			i = encodeVarintExperiment(dAtA, i, uint64(size))
		}
		i--
		dAtA[i] = 0x2a
	}
	if len(m.MetricSpecs) > 0 {
		for iNdEx := len(m.MetricSpecs) - 1; iNdEx >= 0; iNdEx-- {
			{
				size, err := m.MetricSpecs[iNdEx].MarshalToSizedBuffer(dAtA[:i])
				if err != nil {
					return 0, err
				}
				i -= size
				i = encodeVarintExperiment(dAtA, i, uint64(size))
			}
			i--
			dAtA[i] = 0x22
		}
	}
	if len(m.WorkloadSpecs) > 0 {
		for iNdEx := len(m.WorkloadSpecs) - 1; iNdEx >= 0; iNdEx-- {
			{
				size, err := m.WorkloadSpecs[iNdEx].MarshalToSizedBuffer(dAtA[:i])
				if err != nil {
					return 0, err
				}
				i -= size
				i = encodeVarintExperiment(dAtA, i, uint64(size))
			}
			i--
			dAtA[i] = 0x1a
		}
	}
	if m.VizierSpec != nil {
		{
			size, err := m.VizierSpec.MarshalToSizedBuffer(dAtA[:i])
			if err != nil {
				return 0, err
			}
			i -= size
			i = encodeVarintExperiment(dAtA, i, uint64(size))
		}
		i--
		dAtA[i] = 0x12
	}
	if m.VersionSpec != nil {
		{
			size, err := m.VersionSpec.MarshalToSizedBuffer(dAtA[:i])
			if err != nil {
				return 0, err
			}
			i -= size
			i = encodeVarintExperiment(dAtA, i, uint64(size))
		}
		i--
		dAtA[i] = 0xa
	}
	return len(dAtA) - i, nil
}

func (m *VersionSpec) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *VersionSpec) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *VersionSpec) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	return len(dAtA) - i, nil
}

func (m *WorkloadSpec) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *WorkloadSpec) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *WorkloadSpec) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	return len(dAtA) - i, nil
}

func (m *MetricSpec) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *MetricSpec) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *MetricSpec) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	return len(dAtA) - i, nil
}

func (m *ClusterSpec) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *ClusterSpec) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *ClusterSpec) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	return len(dAtA) - i, nil
}

func (m *Empty) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *Empty) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *Empty) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	return len(dAtA) - i, nil
}

func encodeVarintExperiment(dAtA []byte, offset int, v uint64) int {
	offset -= sovExperiment(v)
	base := offset
	for v >= 1<<7 {
		dAtA[offset] = uint8(v&0x7f | 0x80)
		v >>= 7
		offset++
	}
	dAtA[offset] = uint8(v)
	return base
}
func (m *ExperimentSpec) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if m.VersionSpec != nil {
		l = m.VersionSpec.Size()
		n += 1 + l + sovExperiment(uint64(l))
	}
	if m.VizierSpec != nil {
		l = m.VizierSpec.Size()
		n += 1 + l + sovExperiment(uint64(l))
	}
	if len(m.WorkloadSpecs) > 0 {
		for _, e := range m.WorkloadSpecs {
			l = e.Size()
			n += 1 + l + sovExperiment(uint64(l))
		}
	}
	if len(m.MetricSpecs) > 0 {
		for _, e := range m.MetricSpecs {
			l = e.Size()
			n += 1 + l + sovExperiment(uint64(l))
		}
	}
	if m.ClusterSpec != nil {
		l = m.ClusterSpec.Size()
		n += 1 + l + sovExperiment(uint64(l))
	}
	return n
}

func (m *VersionSpec) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	return n
}

func (m *WorkloadSpec) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	return n
}

func (m *MetricSpec) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	return n
}

func (m *ClusterSpec) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	return n
}

func (m *Empty) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	return n
}

func sovExperiment(x uint64) (n int) {
	return (math_bits.Len64(x|1) + 6) / 7
}
func sozExperiment(x uint64) (n int) {
	return sovExperiment(uint64((x << 1) ^ uint64((int64(x) >> 63))))
}
func (this *ExperimentSpec) String() string {
	if this == nil {
		return "nil"
	}
	repeatedStringForWorkloadSpecs := "[]*WorkloadSpec{"
	for _, f := range this.WorkloadSpecs {
		repeatedStringForWorkloadSpecs += strings.Replace(f.String(), "WorkloadSpec", "WorkloadSpec", 1) + ","
	}
	repeatedStringForWorkloadSpecs += "}"
	repeatedStringForMetricSpecs := "[]*MetricSpec{"
	for _, f := range this.MetricSpecs {
		repeatedStringForMetricSpecs += strings.Replace(f.String(), "MetricSpec", "MetricSpec", 1) + ","
	}
	repeatedStringForMetricSpecs += "}"
	s := strings.Join([]string{`&ExperimentSpec{`,
		`VersionSpec:` + strings.Replace(this.VersionSpec.String(), "VersionSpec", "VersionSpec", 1) + `,`,
		`VizierSpec:` + strings.Replace(this.VizierSpec.String(), "WorkloadSpec", "WorkloadSpec", 1) + `,`,
		`WorkloadSpecs:` + repeatedStringForWorkloadSpecs + `,`,
		`MetricSpecs:` + repeatedStringForMetricSpecs + `,`,
		`ClusterSpec:` + strings.Replace(this.ClusterSpec.String(), "ClusterSpec", "ClusterSpec", 1) + `,`,
		`}`,
	}, "")
	return s
}
func (this *VersionSpec) String() string {
	if this == nil {
		return "nil"
	}
	s := strings.Join([]string{`&VersionSpec{`,
		`}`,
	}, "")
	return s
}
func (this *WorkloadSpec) String() string {
	if this == nil {
		return "nil"
	}
	s := strings.Join([]string{`&WorkloadSpec{`,
		`}`,
	}, "")
	return s
}
func (this *MetricSpec) String() string {
	if this == nil {
		return "nil"
	}
	s := strings.Join([]string{`&MetricSpec{`,
		`}`,
	}, "")
	return s
}
func (this *ClusterSpec) String() string {
	if this == nil {
		return "nil"
	}
	s := strings.Join([]string{`&ClusterSpec{`,
		`}`,
	}, "")
	return s
}
func (this *Empty) String() string {
	if this == nil {
		return "nil"
	}
	s := strings.Join([]string{`&Empty{`,
		`}`,
	}, "")
	return s
}
func valueToStringExperiment(v interface{}) string {
	rv := reflect.ValueOf(v)
	if rv.IsNil() {
		return "nil"
	}
	pv := reflect.Indirect(rv).Interface()
	return fmt.Sprintf("*%v", pv)
}
func (m *ExperimentSpec) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowExperiment
			}
			if iNdEx >= l {
				return io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= uint64(b&0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		fieldNum := int32(wire >> 3)
		wireType := int(wire & 0x7)
		if wireType == 4 {
			return fmt.Errorf("proto: ExperimentSpec: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: ExperimentSpec: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		case 1:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field VersionSpec", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowExperiment
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthExperiment
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthExperiment
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			if m.VersionSpec == nil {
				m.VersionSpec = &VersionSpec{}
			}
			if err := m.VersionSpec.Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			iNdEx = postIndex
		case 2:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field VizierSpec", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowExperiment
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthExperiment
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthExperiment
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			if m.VizierSpec == nil {
				m.VizierSpec = &WorkloadSpec{}
			}
			if err := m.VizierSpec.Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			iNdEx = postIndex
		case 3:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field WorkloadSpecs", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowExperiment
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthExperiment
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthExperiment
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			m.WorkloadSpecs = append(m.WorkloadSpecs, &WorkloadSpec{})
			if err := m.WorkloadSpecs[len(m.WorkloadSpecs)-1].Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			iNdEx = postIndex
		case 4:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field MetricSpecs", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowExperiment
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthExperiment
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthExperiment
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			m.MetricSpecs = append(m.MetricSpecs, &MetricSpec{})
			if err := m.MetricSpecs[len(m.MetricSpecs)-1].Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			iNdEx = postIndex
		case 5:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field ClusterSpec", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowExperiment
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				msglen |= int(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if msglen < 0 {
				return ErrInvalidLengthExperiment
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthExperiment
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			if m.ClusterSpec == nil {
				m.ClusterSpec = &ClusterSpec{}
			}
			if err := m.ClusterSpec.Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			iNdEx = postIndex
		default:
			iNdEx = preIndex
			skippy, err := skipExperiment(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthExperiment
			}
			if (iNdEx + skippy) > l {
				return io.ErrUnexpectedEOF
			}
			iNdEx += skippy
		}
	}

	if iNdEx > l {
		return io.ErrUnexpectedEOF
	}
	return nil
}
func (m *VersionSpec) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowExperiment
			}
			if iNdEx >= l {
				return io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= uint64(b&0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		fieldNum := int32(wire >> 3)
		wireType := int(wire & 0x7)
		if wireType == 4 {
			return fmt.Errorf("proto: VersionSpec: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: VersionSpec: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		default:
			iNdEx = preIndex
			skippy, err := skipExperiment(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthExperiment
			}
			if (iNdEx + skippy) > l {
				return io.ErrUnexpectedEOF
			}
			iNdEx += skippy
		}
	}

	if iNdEx > l {
		return io.ErrUnexpectedEOF
	}
	return nil
}
func (m *WorkloadSpec) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowExperiment
			}
			if iNdEx >= l {
				return io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= uint64(b&0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		fieldNum := int32(wire >> 3)
		wireType := int(wire & 0x7)
		if wireType == 4 {
			return fmt.Errorf("proto: WorkloadSpec: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: WorkloadSpec: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		default:
			iNdEx = preIndex
			skippy, err := skipExperiment(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthExperiment
			}
			if (iNdEx + skippy) > l {
				return io.ErrUnexpectedEOF
			}
			iNdEx += skippy
		}
	}

	if iNdEx > l {
		return io.ErrUnexpectedEOF
	}
	return nil
}
func (m *MetricSpec) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowExperiment
			}
			if iNdEx >= l {
				return io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= uint64(b&0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		fieldNum := int32(wire >> 3)
		wireType := int(wire & 0x7)
		if wireType == 4 {
			return fmt.Errorf("proto: MetricSpec: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: MetricSpec: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		default:
			iNdEx = preIndex
			skippy, err := skipExperiment(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthExperiment
			}
			if (iNdEx + skippy) > l {
				return io.ErrUnexpectedEOF
			}
			iNdEx += skippy
		}
	}

	if iNdEx > l {
		return io.ErrUnexpectedEOF
	}
	return nil
}
func (m *ClusterSpec) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowExperiment
			}
			if iNdEx >= l {
				return io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= uint64(b&0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		fieldNum := int32(wire >> 3)
		wireType := int(wire & 0x7)
		if wireType == 4 {
			return fmt.Errorf("proto: ClusterSpec: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: ClusterSpec: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		default:
			iNdEx = preIndex
			skippy, err := skipExperiment(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthExperiment
			}
			if (iNdEx + skippy) > l {
				return io.ErrUnexpectedEOF
			}
			iNdEx += skippy
		}
	}

	if iNdEx > l {
		return io.ErrUnexpectedEOF
	}
	return nil
}
func (m *Empty) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowExperiment
			}
			if iNdEx >= l {
				return io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= uint64(b&0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		fieldNum := int32(wire >> 3)
		wireType := int(wire & 0x7)
		if wireType == 4 {
			return fmt.Errorf("proto: Empty: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: Empty: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		default:
			iNdEx = preIndex
			skippy, err := skipExperiment(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthExperiment
			}
			if (iNdEx + skippy) > l {
				return io.ErrUnexpectedEOF
			}
			iNdEx += skippy
		}
	}

	if iNdEx > l {
		return io.ErrUnexpectedEOF
	}
	return nil
}
func skipExperiment(dAtA []byte) (n int, err error) {
	l := len(dAtA)
	iNdEx := 0
	depth := 0
	for iNdEx < l {
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return 0, ErrIntOverflowExperiment
			}
			if iNdEx >= l {
				return 0, io.ErrUnexpectedEOF
			}
			b := dAtA[iNdEx]
			iNdEx++
			wire |= (uint64(b) & 0x7F) << shift
			if b < 0x80 {
				break
			}
		}
		wireType := int(wire & 0x7)
		switch wireType {
		case 0:
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return 0, ErrIntOverflowExperiment
				}
				if iNdEx >= l {
					return 0, io.ErrUnexpectedEOF
				}
				iNdEx++
				if dAtA[iNdEx-1] < 0x80 {
					break
				}
			}
		case 1:
			iNdEx += 8
		case 2:
			var length int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return 0, ErrIntOverflowExperiment
				}
				if iNdEx >= l {
					return 0, io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				length |= (int(b) & 0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			if length < 0 {
				return 0, ErrInvalidLengthExperiment
			}
			iNdEx += length
		case 3:
			depth++
		case 4:
			if depth == 0 {
				return 0, ErrUnexpectedEndOfGroupExperiment
			}
			depth--
		case 5:
			iNdEx += 4
		default:
			return 0, fmt.Errorf("proto: illegal wireType %d", wireType)
		}
		if iNdEx < 0 {
			return 0, ErrInvalidLengthExperiment
		}
		if depth == 0 {
			return iNdEx, nil
		}
	}
	return 0, io.ErrUnexpectedEOF
}

var (
	ErrInvalidLengthExperiment        = fmt.Errorf("proto: negative length found during unmarshaling")
	ErrIntOverflowExperiment          = fmt.Errorf("proto: integer overflow")
	ErrUnexpectedEndOfGroupExperiment = fmt.Errorf("proto: unexpected end of group")
)