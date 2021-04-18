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

package components

import (
	"fmt"
	"os"
	"sync"

	"github.com/fatih/color"
)

const dragon = `
================================================================================

         __        _
       _/  \    _(\(o         CUSTOMER CLUSTER: %s
      /     \  /  _  ^^^o
     /   !   \/  ! '!!!v'     BE CAREFUL, WORKING WITH CUSTOMER CLUSTER.
    !  !  \ _' ( \____        Bureaucrat Dragon Rules:
    ! . \ _!\   \===^\)         DO NOT SHARE DATA!
     \ \_!  / __!               DO NOT SHARE ON SLACK CHANNELS!
      \!   /    \               DO NOT LOGIN WITHOUT EXPLICIT PERMISSION.
(\_      _/   _\ )              BE CAREFUL!!!
 \ ^^--^^ __-^ /(__
  ^^----^^    "^--v'
================================================================================
`

// This is a bit of hack to make sure this only get's rendered once.
// We can remove this when we clean up the auth login in the CLI.
var once = sync.Once{}

// RenderBureaucratDragon will write out the dragon once, per session.
func RenderBureaucratDragon(orgName string) {
	once.Do(func() {
		fmt.Fprint(os.Stderr, color.RedString(dragon, orgName))
	})
}
