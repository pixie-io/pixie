// This executable is only for testing purposes.
// We use it to see if we can find the function symbols and debug information.

package main

import (
	"fmt"
	"math"
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

func MixedArgTypes(i1 int, b1 bool, b2 BoolWrapper, i2 int, i3 int, b3 bool) int {
	if (b1 && (b2.B0 || b2.B3) && b3) {
	    return 0
	}
	return i1*i2*i3
}

func main() {
	v := Vertex{3, 4}
	v2 := Vertex{2, 9}
	v.CrossScale(v2, 10)
	fmt.Println(v.Abs())
	fmt.Println(MixedArgTypes(1, true, BoolWrapper{false, true, false, true}, 4, 5, false));
}
