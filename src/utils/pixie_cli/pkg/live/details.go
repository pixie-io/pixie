package live

import "github.com/rivo/tview"

type detailsModal struct {
	s  string
	tv *tview.TextView
}

func newDetailsModal(s string) *detailsModal {
	return &detailsModal{
		s: s,
	}
}

// Show shows the modal.
func (m *detailsModal) Show(app *tview.Application) tview.Primitive {
	m.tv = tview.NewTextView()
	m.tv.SetDynamicColors(true).
		SetText(m.s).
		SetBorder(true)
	app.SetFocus(m.tv)
	return m.tv
}

// Close is called when the modal is closed.
func (m *detailsModal) Close(app *tview.Application) {
	m.tv = nil
}
