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
	cryptoRand "crypto/rand"
	"crypto/rsa"
	"fmt"
	"log"
	"math/rand"
	"os"
	"strconv"
	"time"

	"net/http"
	_ "net/http/pprof"
)

type env struct {
	callDepth    int
	numFunctions int
	pauseTimeNS  int
}

func (e *env) getRandom(numCalls int) func(int) {
	if numCalls >= e.callDepth {
		return func(int) {
			privateKey, err := rsa.GenerateKey(cryptoRand.Reader, 4096)
			_ = privateKey
			if err != nil {
				panic(err)
			}
			time.Sleep(time.Duration(e.pauseTimeNS) * time.Nanosecond)
		}
	}

	n := rand.Intn(e.numFunctions)
	switch n {
	case 0:
		return e.somewhatLongFunctionName0
	case 1:
		return e.somewhatLongFunctionName1
	case 2:
		return e.somewhatLongFunctionName2
	case 3:
		return e.somewhatLongFunctionName3
	case 4:
		return e.somewhatLongFunctionName4
	case 5:
		return e.somewhatLongFunctionName5
	case 6:
		return e.somewhatLongFunctionName6
	case 7:
		return e.somewhatLongFunctionName7
	case 8:
		return e.somewhatLongFunctionName8
	case 9:
		return e.somewhatLongFunctionName9
	case 10:
		return e.somewhatLongFunctionName10
	case 11:
		return e.somewhatLongFunctionName11
	case 12:
		return e.somewhatLongFunctionName12
	case 13:
		return e.somewhatLongFunctionName13
	case 14:
		return e.somewhatLongFunctionName14
	case 15:
		return e.somewhatLongFunctionName15
	case 16:
		return e.somewhatLongFunctionName16
	case 17:
		return e.somewhatLongFunctionName17
	case 18:
		return e.somewhatLongFunctionName18
	case 19:
		return e.somewhatLongFunctionName19
	case 20:
		return e.somewhatLongFunctionName20
	case 21:
		return e.somewhatLongFunctionName21
	case 22:
		return e.somewhatLongFunctionName22
	case 23:
		return e.somewhatLongFunctionName23
	case 24:
		return e.somewhatLongFunctionName24
	case 25:
		return e.somewhatLongFunctionName25
	case 26:
		return e.somewhatLongFunctionName26
	case 27:
		return e.somewhatLongFunctionName27
	case 28:
		return e.somewhatLongFunctionName28
	case 29:
		return e.somewhatLongFunctionName29
	}
	panic("Shouldn't have gotten here")
}

//go:noinline
func (e *env) somewhatLongFunctionName0(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName1(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName2(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName3(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName4(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName5(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName6(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName7(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName8(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName9(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName10(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName11(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName12(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName13(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName14(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName15(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName16(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName17(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName18(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName19(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName20(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName21(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName22(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName23(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName24(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName25(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName26(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName27(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName28(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

//go:noinline
func (e *env) somewhatLongFunctionName29(numCalls int) {
	somewhatLongFunctionName := e.getRandom(numCalls + 1)
	somewhatLongFunctionName(numCalls + 1)
}

func main() {
	goroutines, err := strconv.Atoi(os.Getenv("NUM_GOROUTINES"))
	if err != nil {
		panic(err)
	}
	pauseTimeNS, err := strconv.Atoi(os.Getenv("PAUSE_TIME_NS"))
	if err != nil {
		panic(err)
	}
	numFunctions, err := strconv.Atoi(os.Getenv("NUM_FUNCTIONS"))
	if err != nil {
		panic(err)
	}
	if numFunctions > 30 {
		panic(fmt.Sprintf("NUM_FUNCTIONS was %d but cannot exceed 30", numFunctions))
	}

	callDepth, err := strconv.Atoi(os.Getenv("CALL_STACK_DEPTH"))
	if err != nil {
		panic(err)
	}

	e := &env{
		callDepth:    callDepth,
		numFunctions: numFunctions,
		pauseTimeNS:  pauseTimeNS,
	}

	for i := 0; i < goroutines; i++ {
		go func() {
			for {
				somewhatLongFunctionName := e.getRandom(0)
				somewhatLongFunctionName(0)
			}
		}()
	}

	log.Println(http.ListenAndServe("localhost:6060", nil))
}
