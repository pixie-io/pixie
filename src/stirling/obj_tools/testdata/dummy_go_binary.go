// This executable is only for testing purposes.
// We use it to see if we can find the function symbols and debug information.

package main

import (
	"fmt"
	"math"
	"math/rand"
	"time"
	"encoding/hex"
)

type Vertex struct {
	X float32
	Y float64
}

// This struct is interesting to test alignment code.
// It is a 4-byte struct, but can be aligned on any byte boundary.
type BoolWrapper struct {
	B0 bool
	B1 bool
	B2 bool
	B3 bool
}

type PointerWrapper struct {
	V0  int64
	V1  int64
	V2  int64
	Ptr *int
}

type PointerWrapperWrapper struct {
	V0  int64
	V1  int64
	Val PointerWrapper
	V2  int64
}

type PointerWrapperWrapperWrapper struct {
	V0  int64
	Ptr *PointerWrapperWrapper
	V1  int64
	V2  int64
}

func PointerWrapperWrapperWrapperFunc(p PointerWrapperWrapperWrapper) int {
	return *p.Ptr.Val.Ptr // *(p.Ptr->Val.Ptr)

}

func (v Vertex) Abs() float64 {
	return math.Sqrt(float64(v.X*v.X) + v.Y*v.Y)
}

func (v *Vertex) Scale(f float64) {
	v.X = v.X * float32(f)
	v.Y = v.Y * f
}

func (v *Vertex) CrossScale(v2 Vertex, f float64) {
	v.X = v.X * v2.X
	v.Y = v.Y * v2.Y
	v.Scale(f)
}

func MixedArgTypes(i1 int, b1 bool, b2 BoolWrapper, i2 int, i3 int, b3 bool) (int, BoolWrapper) {
	if b1 && (b2.B0 || b2.B3) && b3 {
		return 7, BoolWrapper{true, false, true, false}
	}
	return i1 * i2 * i3, BoolWrapper{true, false, true, false}
}

func GoHasNamedReturns() (retfoo int, retbar bool) {
	return 12, true
}

func SaySomethingTo(something string, name string) string {
	return something + ", " + name
}

func Uint8ArrayToHex(uuid []uint8) string {
	return hex.EncodeToString(uuid)
}

func BytesToHex(uuid []byte) string {
	return hex.EncodeToString(uuid)
}

func main() {
	for true {
		v := Vertex{3, 4}
		v2 := Vertex{2, 9}
		v.CrossScale(v2, 10)
		fmt.Println(v.Abs())
		fmt.Println(MixedArgTypes(
			rand.Intn(100),
			rand.Intn(2) == 0,
			BoolWrapper{rand.Intn(2) == 0, rand.Intn(2) == 0, rand.Intn(2) == 0, rand.Intn(2) == 0},
			rand.Intn(100),
			rand.Intn(100),
			rand.Intn(2) == 0))
		fmt.Println(GoHasNamedReturns())

		a := 5
		b := PointerWrapper{1, 2, 3, &a}
		c := PointerWrapperWrapper{1, 2, b, 3}
		d := PointerWrapperWrapperWrapper{1, &c, 2, 3}
		fmt.Println(PointerWrapperWrapperWrapperFunc(d))
		fmt.Println(SaySomethingTo("Hello", "pixienaut"));

		id0 := []uint8{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}
		fmt.Println(Uint8ArrayToHex(id0))

		id1 := []byte{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}
        fmt.Println(BytesToHex(id1))

		time.Sleep(time.Second)
	}
}
