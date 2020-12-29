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
