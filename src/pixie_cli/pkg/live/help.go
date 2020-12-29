package live

import (
	"fmt"
	"strings"

	"github.com/rivo/tview"
)

type helpModal struct{}

// Show is returns the modal primitive.
func (h *helpModal) Show(app *tview.Application) tview.Primitive {
	m := tview.NewTextView()
	m.SetDynamicColors(true)
	m.SetBorderPadding(1, 1, 1, 1)
	m.SetBorder(true)

	type shortcut struct {
		keySequence []string
		desc        string
	}

	shortcuts := []shortcut{
		{[]string{"?"}, "Show this help menu"},
		{[]string{"ctrl", "s"}, "Search for text (\"/\")"},
		{[]string{"ctrl", "k"}, "Show Pixie command menu"},
		{[]string{"ctrl", "c"}, "Quit the application"},
		{[]string{"ctrl", "v"}, "View the underlying script"},
		{[]string{"ctrl", "r"}, "Run current script (again)"},
		{[]string{"escape"}, "Close dialogs/modals"},
	}

	maxWidth := 0
	for _, s := range shortcuts {
		w := 0
		for _, ks := range s.keySequence {
			w += 1 + len(ks)
		}
		if maxWidth < w {
			maxWidth = w
		}
	}

	fmt.Fprintf(m, "%s\n", withAccent("Keyboard Shortcuts:\n"))
	for _, s := range shortcuts {
		w := 0
		accented := make([]string, len(s.keySequence))
		for i, k := range s.keySequence {
			w += 1 + len(k)
			accented[i] = withAccent(k)
		}
		fmt.Fprintf(m, "%s : %s\n",
			strings.Repeat(" ", maxWidth-w)+strings.Join(accented, "+"),
			s.desc)
	}
	app.SetFocus(m)
	return m
}

// Close is called when modal is destroyed.
func (h *helpModal) Close(app *tview.Application) {}
