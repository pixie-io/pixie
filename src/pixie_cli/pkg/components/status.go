package components

import (
	"strings"

	"github.com/fatih/color"
)

var (
	statusOK  = "\u2714"
	statusErr = "\u2715"
)

func computePadding(s string, pad int) (padS int, padE int) {
	pad -= len([]rune(s))
	if pad < 0 {
		pad = 0
	}

	padS = int(pad / 2)
	padE = padS
	if pad%2 != 0 {
		padE = padS + 1
	}
	return
}

// StatusOK prints out the default OK symbol, center padded to the size specified.
func StatusOK(pad int) string {
	padS, padE := computePadding(statusOK, pad)
	return strings.Repeat(" ", padS) + color.GreenString(statusOK) + strings.Repeat(" ", padE)
}

// StatusErr prints out the default Err symbol, center padded to the size specified.
func StatusErr(pad int) string {
	padS, padE := computePadding(statusErr, pad)
	return strings.Repeat(" ", padS) + color.RedString(statusErr) + strings.Repeat(" ", padE)
}
