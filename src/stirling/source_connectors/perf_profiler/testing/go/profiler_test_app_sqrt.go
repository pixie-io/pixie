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

package main

import (
	"fmt"
	"math"
	"os"
	"os/signal"
	"syscall"
)

func sqrt(x float64) float64 {
	var r = x
	var precision = 1e-10
	var eps = math.Abs(x - r*r)
	for eps > precision {
		r = (r + x/r) / 2
		eps = math.Abs(x - r*r)
	}
	return r
}

func sqrtOf1e39() float64 {
	// Runs for 70 iters.
	var x = 1e39
	return sqrt(x)
}

func sqrtOf1e18() float64 {
	// Runs for 35 iters.
	var x = 1e18
	return sqrt(x)
}

func main() {
	sigs := make(chan os.Signal, 1)
	signal.Notify(sigs, syscall.SIGINT, syscall.SIGTERM)

	var done = false
	var x float64
	var y float64

	go func() {
		// A goroutine that blocks on the receipt of a signal;
		// after the signal is received, we set done to true
		// so that the main loop terminates.
		<-sigs
		fmt.Println("")
		fmt.Println("done")
		done = true
	}()

	for !done {
		x = sqrtOf1e39()
		y = sqrtOf1e18()
	}
	fmt.Println("sqrtOf1e39():", x)
	fmt.Println("sqrtOf1e18():", y)
}
