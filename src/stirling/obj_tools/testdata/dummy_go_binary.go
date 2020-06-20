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

func main() {
	v := Vertex{3, 4}
	v2 := Vertex{2, 9}
	v.CrossScale(v2, 10)
	fmt.Println(v.Abs())
}