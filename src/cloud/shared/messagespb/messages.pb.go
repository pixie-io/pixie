// Code generated by protoc-gen-gogo. DO NOT EDIT.
// source: src/cloud/shared/messagespb/messages.proto

package messagespb

import (
	fmt "fmt"
	_ "github.com/gogo/protobuf/gogoproto"
	proto "github.com/gogo/protobuf/proto"
	io "io"
	math "math"
	math_bits "math/bits"
	uuidpb "px.dev/pixie/src/api/proto/uuidpb"
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

type VizierConnected struct {
	VizierID *uuidpb.UUID `protobuf:"bytes,1,opt,name=vizier_id,json=vizierId,proto3" json:"vizier_id,omitempty"`
	OrgID    *uuidpb.UUID `protobuf:"bytes,2,opt,name=org_id,json=orgId,proto3" json:"org_id,omitempty"`
	K8sUID   string       `protobuf:"bytes,4,opt,name=k8s_uid,json=k8sUid,proto3" json:"k8s_uid,omitempty"`
}

func (m *VizierConnected) Reset()      { *m = VizierConnected{} }
func (*VizierConnected) ProtoMessage() {}
func (*VizierConnected) Descriptor() ([]byte, []int) {
	return fileDescriptor_149ebc172e39bd73, []int{0}
}
func (m *VizierConnected) XXX_Unmarshal(b []byte) error {
	return m.Unmarshal(b)
}
func (m *VizierConnected) XXX_Marshal(b []byte, deterministic bool) ([]byte, error) {
	if deterministic {
		return xxx_messageInfo_VizierConnected.Marshal(b, m, deterministic)
	} else {
		b = b[:cap(b)]
		n, err := m.MarshalToSizedBuffer(b)
		if err != nil {
			return nil, err
		}
		return b[:n], nil
	}
}
func (m *VizierConnected) XXX_Merge(src proto.Message) {
	xxx_messageInfo_VizierConnected.Merge(m, src)
}
func (m *VizierConnected) XXX_Size() int {
	return m.Size()
}
func (m *VizierConnected) XXX_DiscardUnknown() {
	xxx_messageInfo_VizierConnected.DiscardUnknown(m)
}

var xxx_messageInfo_VizierConnected proto.InternalMessageInfo

func (m *VizierConnected) GetVizierID() *uuidpb.UUID {
	if m != nil {
		return m.VizierID
	}
	return nil
}

func (m *VizierConnected) GetOrgID() *uuidpb.UUID {
	if m != nil {
		return m.OrgID
	}
	return nil
}

func (m *VizierConnected) GetK8sUID() string {
	if m != nil {
		return m.K8sUID
	}
	return ""
}

func init() {
	proto.RegisterType((*VizierConnected)(nil), "px.cloud.shared.messages.VizierConnected")
}

func init() {
	proto.RegisterFile("src/cloud/shared/messagespb/messages.proto", fileDescriptor_149ebc172e39bd73)
}

var fileDescriptor_149ebc172e39bd73 = []byte{
	// 311 bytes of a gzipped FileDescriptorProto
	0x1f, 0x8b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0xff, 0x6c, 0x90, 0xb1, 0x4a, 0x03, 0x31,
	0x18, 0xc7, 0x2f, 0xda, 0x9e, 0x6d, 0x14, 0x2a, 0x87, 0x43, 0xe9, 0xf0, 0x5d, 0xd1, 0xa5, 0x38,
	0xe4, 0x50, 0x97, 0xe2, 0x24, 0xf5, 0x96, 0xe8, 0x20, 0x14, 0xce, 0xc1, 0xa5, 0xb4, 0x4d, 0x88,
	0xa1, 0xda, 0x84, 0xa4, 0x27, 0xc5, 0xc9, 0x47, 0xf0, 0x31, 0x04, 0x5f, 0xc4, 0xb1, 0x63, 0xa7,
	0x62, 0xd3, 0xc5, 0xb1, 0x8f, 0x20, 0xbd, 0xa0, 0x2e, 0x4e, 0xf9, 0x93, 0xff, 0xef, 0xf7, 0xc1,
	0xf7, 0xe1, 0x63, 0x6b, 0x86, 0xc9, 0xf0, 0x41, 0xe5, 0x2c, 0xb1, 0xf7, 0x7d, 0xc3, 0x59, 0xf2,
	0xc8, 0xad, 0xed, 0x0b, 0x6e, 0xf5, 0xe0, 0x37, 0x12, 0x6d, 0xd4, 0x44, 0x45, 0x75, 0x3d, 0x25,
	0x05, 0x4a, 0x3c, 0x4a, 0x7e, 0xfa, 0xc6, 0x81, 0x50, 0x42, 0x15, 0x50, 0xb2, 0x49, 0x9e, 0x6f,
	0xc4, 0x9b, 0xd9, 0x7d, 0x2d, 0x13, 0xdf, 0xe4, 0xb9, 0x64, 0x7a, 0x50, 0x3c, 0x1e, 0x38, 0x7c,
	0x47, 0xb8, 0x76, 0x2b, 0x9f, 0x25, 0x37, 0x97, 0x6a, 0x3c, 0xe6, 0xc3, 0x09, 0x67, 0xd1, 0x39,
	0xae, 0x3e, 0x15, 0x5f, 0x3d, 0xc9, 0xea, 0xa8, 0x89, 0x5a, 0xbb, 0xa7, 0x35, 0xa2, 0xa7, 0xc4,
	0xdb, 0x24, 0xcb, 0x68, 0xda, 0xd9, 0x73, 0x8b, 0xb8, 0xe2, 0x45, 0x9a, 0x76, 0x2b, 0x9e, 0xa7,
	0x2c, 0x3a, 0xc1, 0xa1, 0x32, 0x62, 0x23, 0x6e, 0xfd, 0x2f, 0x56, 0xdd, 0x22, 0x2e, 0xdf, 0x18,
	0x41, 0xd3, 0x6e, 0x59, 0x19, 0x41, 0x59, 0x74, 0x84, 0x77, 0x46, 0x6d, 0xdb, 0xcb, 0x25, 0xab,
	0x97, 0x9a, 0xa8, 0x55, 0xed, 0x60, 0xb7, 0x88, 0xc3, 0xeb, 0xb6, 0xcd, 0x68, 0xda, 0x0d, 0x47,
	0x6d, 0x9b, 0x49, 0x76, 0x55, 0xaa, 0x6c, 0xef, 0x97, 0x3a, 0x17, 0xb3, 0x25, 0x04, 0xf3, 0x25,
	0x04, 0xeb, 0x25, 0xa0, 0x17, 0x07, 0xe8, 0xcd, 0x01, 0xfa, 0x70, 0x80, 0x66, 0x0e, 0xd0, 0xa7,
	0x03, 0xf4, 0xe5, 0x20, 0x58, 0x3b, 0x40, 0xaf, 0x2b, 0x08, 0x66, 0x2b, 0x08, 0xe6, 0x2b, 0x08,
	0xee, 0xf0, 0xdf, 0x3d, 0x07, 0x61, 0xb1, 0xf6, 0xd9, 0x77, 0x00, 0x00, 0x00, 0xff, 0xff, 0x56,
	0xd8, 0xd9, 0x7e, 0x75, 0x01, 0x00, 0x00,
}

func (this *VizierConnected) Equal(that interface{}) bool {
	if that == nil {
		return this == nil
	}

	that1, ok := that.(*VizierConnected)
	if !ok {
		that2, ok := that.(VizierConnected)
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
	if !this.VizierID.Equal(that1.VizierID) {
		return false
	}
	if !this.OrgID.Equal(that1.OrgID) {
		return false
	}
	if this.K8sUID != that1.K8sUID {
		return false
	}
	return true
}
func (this *VizierConnected) GoString() string {
	if this == nil {
		return "nil"
	}
	s := make([]string, 0, 7)
	s = append(s, "&messagespb.VizierConnected{")
	if this.VizierID != nil {
		s = append(s, "VizierID: "+fmt.Sprintf("%#v", this.VizierID)+",\n")
	}
	if this.OrgID != nil {
		s = append(s, "OrgID: "+fmt.Sprintf("%#v", this.OrgID)+",\n")
	}
	s = append(s, "K8sUID: "+fmt.Sprintf("%#v", this.K8sUID)+",\n")
	s = append(s, "}")
	return strings.Join(s, "")
}
func valueToGoStringMessages(v interface{}, typ string) string {
	rv := reflect.ValueOf(v)
	if rv.IsNil() {
		return "nil"
	}
	pv := reflect.Indirect(rv).Interface()
	return fmt.Sprintf("func(v %v) *%v { return &v } ( %#v )", typ, typ, pv)
}
func (m *VizierConnected) Marshal() (dAtA []byte, err error) {
	size := m.Size()
	dAtA = make([]byte, size)
	n, err := m.MarshalToSizedBuffer(dAtA[:size])
	if err != nil {
		return nil, err
	}
	return dAtA[:n], nil
}

func (m *VizierConnected) MarshalTo(dAtA []byte) (int, error) {
	size := m.Size()
	return m.MarshalToSizedBuffer(dAtA[:size])
}

func (m *VizierConnected) MarshalToSizedBuffer(dAtA []byte) (int, error) {
	i := len(dAtA)
	_ = i
	var l int
	_ = l
	if len(m.K8sUID) > 0 {
		i -= len(m.K8sUID)
		copy(dAtA[i:], m.K8sUID)
		i = encodeVarintMessages(dAtA, i, uint64(len(m.K8sUID)))
		i--
		dAtA[i] = 0x22
	}
	if m.OrgID != nil {
		{
			size, err := m.OrgID.MarshalToSizedBuffer(dAtA[:i])
			if err != nil {
				return 0, err
			}
			i -= size
			i = encodeVarintMessages(dAtA, i, uint64(size))
		}
		i--
		dAtA[i] = 0x12
	}
	if m.VizierID != nil {
		{
			size, err := m.VizierID.MarshalToSizedBuffer(dAtA[:i])
			if err != nil {
				return 0, err
			}
			i -= size
			i = encodeVarintMessages(dAtA, i, uint64(size))
		}
		i--
		dAtA[i] = 0xa
	}
	return len(dAtA) - i, nil
}

func encodeVarintMessages(dAtA []byte, offset int, v uint64) int {
	offset -= sovMessages(v)
	base := offset
	for v >= 1<<7 {
		dAtA[offset] = uint8(v&0x7f | 0x80)
		v >>= 7
		offset++
	}
	dAtA[offset] = uint8(v)
	return base
}
func (m *VizierConnected) Size() (n int) {
	if m == nil {
		return 0
	}
	var l int
	_ = l
	if m.VizierID != nil {
		l = m.VizierID.Size()
		n += 1 + l + sovMessages(uint64(l))
	}
	if m.OrgID != nil {
		l = m.OrgID.Size()
		n += 1 + l + sovMessages(uint64(l))
	}
	l = len(m.K8sUID)
	if l > 0 {
		n += 1 + l + sovMessages(uint64(l))
	}
	return n
}

func sovMessages(x uint64) (n int) {
	return (math_bits.Len64(x|1) + 6) / 7
}
func sozMessages(x uint64) (n int) {
	return sovMessages(uint64((x << 1) ^ uint64((int64(x) >> 63))))
}
func (this *VizierConnected) String() string {
	if this == nil {
		return "nil"
	}
	s := strings.Join([]string{`&VizierConnected{`,
		`VizierID:` + strings.Replace(fmt.Sprintf("%v", this.VizierID), "UUID", "uuidpb.UUID", 1) + `,`,
		`OrgID:` + strings.Replace(fmt.Sprintf("%v", this.OrgID), "UUID", "uuidpb.UUID", 1) + `,`,
		`K8sUID:` + fmt.Sprintf("%v", this.K8sUID) + `,`,
		`}`,
	}, "")
	return s
}
func valueToStringMessages(v interface{}) string {
	rv := reflect.ValueOf(v)
	if rv.IsNil() {
		return "nil"
	}
	pv := reflect.Indirect(rv).Interface()
	return fmt.Sprintf("*%v", pv)
}
func (m *VizierConnected) Unmarshal(dAtA []byte) error {
	l := len(dAtA)
	iNdEx := 0
	for iNdEx < l {
		preIndex := iNdEx
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return ErrIntOverflowMessages
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
			return fmt.Errorf("proto: VizierConnected: wiretype end group for non-group")
		}
		if fieldNum <= 0 {
			return fmt.Errorf("proto: VizierConnected: illegal tag %d (wire type %d)", fieldNum, wire)
		}
		switch fieldNum {
		case 1:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field VizierID", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowMessages
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
				return ErrInvalidLengthMessages
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthMessages
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			if m.VizierID == nil {
				m.VizierID = &uuidpb.UUID{}
			}
			if err := m.VizierID.Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			iNdEx = postIndex
		case 2:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field OrgID", wireType)
			}
			var msglen int
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowMessages
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
				return ErrInvalidLengthMessages
			}
			postIndex := iNdEx + msglen
			if postIndex < 0 {
				return ErrInvalidLengthMessages
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			if m.OrgID == nil {
				m.OrgID = &uuidpb.UUID{}
			}
			if err := m.OrgID.Unmarshal(dAtA[iNdEx:postIndex]); err != nil {
				return err
			}
			iNdEx = postIndex
		case 4:
			if wireType != 2 {
				return fmt.Errorf("proto: wrong wireType = %d for field K8sUID", wireType)
			}
			var stringLen uint64
			for shift := uint(0); ; shift += 7 {
				if shift >= 64 {
					return ErrIntOverflowMessages
				}
				if iNdEx >= l {
					return io.ErrUnexpectedEOF
				}
				b := dAtA[iNdEx]
				iNdEx++
				stringLen |= uint64(b&0x7F) << shift
				if b < 0x80 {
					break
				}
			}
			intStringLen := int(stringLen)
			if intStringLen < 0 {
				return ErrInvalidLengthMessages
			}
			postIndex := iNdEx + intStringLen
			if postIndex < 0 {
				return ErrInvalidLengthMessages
			}
			if postIndex > l {
				return io.ErrUnexpectedEOF
			}
			m.K8sUID = string(dAtA[iNdEx:postIndex])
			iNdEx = postIndex
		default:
			iNdEx = preIndex
			skippy, err := skipMessages(dAtA[iNdEx:])
			if err != nil {
				return err
			}
			if (skippy < 0) || (iNdEx+skippy) < 0 {
				return ErrInvalidLengthMessages
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
func skipMessages(dAtA []byte) (n int, err error) {
	l := len(dAtA)
	iNdEx := 0
	depth := 0
	for iNdEx < l {
		var wire uint64
		for shift := uint(0); ; shift += 7 {
			if shift >= 64 {
				return 0, ErrIntOverflowMessages
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
					return 0, ErrIntOverflowMessages
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
					return 0, ErrIntOverflowMessages
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
				return 0, ErrInvalidLengthMessages
			}
			iNdEx += length
		case 3:
			depth++
		case 4:
			if depth == 0 {
				return 0, ErrUnexpectedEndOfGroupMessages
			}
			depth--
		case 5:
			iNdEx += 4
		default:
			return 0, fmt.Errorf("proto: illegal wireType %d", wireType)
		}
		if iNdEx < 0 {
			return 0, ErrInvalidLengthMessages
		}
		if depth == 0 {
			return iNdEx, nil
		}
	}
	return 0, io.ErrUnexpectedEOF
}

var (
	ErrInvalidLengthMessages        = fmt.Errorf("proto: negative length found during unmarshaling")
	ErrIntOverflowMessages          = fmt.Errorf("proto: integer overflow")
	ErrUnexpectedEndOfGroupMessages = fmt.Errorf("proto: unexpected end of group")
)
