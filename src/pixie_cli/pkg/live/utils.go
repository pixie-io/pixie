package live

import (
	"encoding/json"
	"fmt"
	"regexp"
	"strings"

	"github.com/alecthomas/chroma/quick"
	"github.com/rivo/tview"
)

func withAccent(s string) string {
	return fmt.Sprintf("[%s]%s[%s]", accentColor, s, textColor)
}

// Returns a new primitive which puts the provided primitive in the center and
// sets its size to the given width and height.
func createModal(p tview.Primitive, width, height int) tview.Primitive {
	return tview.NewFlex().
		AddItem(nil, 0, 1, false).
		AddItem(tview.NewFlex().SetDirection(tview.FlexRow).
			AddItem(nil, 0, 1, false).
			AddItem(p, height, 1, false).
			AddItem(nil, 0, 1, false), width, 1, false).
		AddItem(nil, 0, 1, false)
}

func contains(needle int, haystack []int) bool {
	for _, i := range haystack {
		if needle == i {
			return true
		}
	}
	return false
}

func tryJSONHighlight(s string) string {
	// Try to parse large blob as a string.
	var res interface{}
	if err := json.Unmarshal([]byte(s), &res); err != nil {
		return s
	}
	parsed, err := json.MarshalIndent(res, "", "  ")
	if err != nil {
		return s
	}

	highlighted := strings.Builder{}
	err = quick.Highlight(&highlighted, string(parsed), "json", "terminal16m", "monokai")
	if err != nil {
		return s
	}
	return highlighted.String()
}

func stripColors(s string) string {
	re := regexp.MustCompile(`\[.*\]`)
	return re.ReplaceAllString(s, "")
}
