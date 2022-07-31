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

package cmd

import (
	"errors"
	"math"

	"gonum.org/v1/gonum/stat/distuv"
)

// UTest performs a normal approximation to a Mann-Whitney U-Test on the given data.
// This approximation is reasonable when the sample sizes are >= 20.
func UTest(a, b []float64) (float64, error) {
	if len(a) < 20 || len(b) < 20 {
		return 0, errors.New("utest normal approximation only valid for sample sizes geq to 20")
	}
	uStat := calcUStat(a, b)
	zStat := normApproxOfU(uStat, len(a), len(b))

	stdNorm := &distuv.Normal{
		Mu:    0,
		Sigma: 1,
	}

	// We do a two-tailed U-test so p should be twice the CDF of the z approximation.
	p := 2 * stdNorm.CDF(zStat)
	return p, nil
}

func calcUStat(a, b []float64) float64 {
	u1 := calcUOneSided(a, b)
	u2 := calcUOneSided(b, a)
	if u1 > u2 {
		return u2
	}
	return u1
}

func calcUOneSided(a, b []float64) float64 {
	u := 0.0
	for _, x := range a {
		for _, y := range b {
			if x > y {
				u += 1.0
			} else if x == y {
				u += 0.5
			}
		}
	}
	return u
}

func normApproxOfU(uStat float64, n1 int, n2 int) float64 {
	// Note that this ignores the tie correction to the approximation.
	// But ties seem very unlikely since this u test is only used for times measured in microseconds.
	mu := float64(n1*n2) / 2
	sigma := math.Sqrt(float64(n1*n2*(n1+n2+1)) / 12)
	z := (uStat - mu) / sigma
	return z
}
