package live

import (
	"fmt"
	"strings"

	"github.com/gdamore/tcell"
	"github.com/rivo/tview"
	"github.com/sahilm/fuzzy"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"
)

type suggestion struct {
	scriptName string
	desc       string

	matchedIndexes []int
}

type autocompleter interface {
	GetSuggestions(string) []suggestion
}

// autcompleteModal is the autocomplete modal.
type autocompleteModal struct {
	// Reference to the parent view.
	s *appState

	// The input box.
	ib *tview.InputField
	// The suggestion list.
	sl *tview.List
	// The description text box.
	dt *tview.TextView

	layout *tview.Flex

	// Current list of suggestions.
	suggestions    []suggestion
	scriptExecFunc func(*script.ExecutableScript)
}

func newAutocompleteModal(st *appState) *autocompleteModal {
	// The auto complete view consists of three widgets.:
	//  ------------------------------------------
	//  | Text box for search                    |
	//  |________________________________________|
	//  |  Suggestions       | Description       |
	//  |  List              |                   |
	//  |                    |                   |
	//  |                    |                   |
	//  |____________________|___________________|
	//
	// The description is updated when a specific suggestion is selection.
	// In the current state, entering tab whil on the text box will pick the
	// first suggestion.
	// Hitting down arrow will move to the suggestions list. Enter when
	// in the suggestions list will make it the active search.

	scriptInputBox := tview.NewInputField()
	scriptInputBox.
		SetBackgroundColor(tcell.ColorBlack)
	scriptInputBox.
		SetFieldBackgroundColor(tcell.ColorBlack).
		SetBorder(true)

	scriptListBox := tview.NewList()
	scriptListBox.
		ShowSecondaryText(false).
		SetBorder(true)

	scriptDescBox := tview.NewTextView()
	scriptDescBox.
		SetDynamicColors(true).
		SetBorder(true)

	// We need two layouts, one going | and the other going --.
	horiz := tview.NewFlex().
		SetDirection(tview.FlexColumn).
		AddItem(scriptListBox, 0, 5, false).
		AddItem(scriptDescBox, 0, 10, false)

	layout := tview.NewFlex().
		SetDirection(tview.FlexRow).
		AddItem(scriptInputBox, 3, 0, true).
		AddItem(horiz, 0, 1, false)

	return &autocompleteModal{
		s:      st,
		ib:     scriptInputBox,
		sl:     scriptListBox,
		dt:     scriptDescBox,
		layout: layout,
	}
}

// Show shows the modal.
func (m *autocompleteModal) Show(app *tview.Application) tview.Primitive {
	// Start with suggestions based on empty input.
	m.suggestions = m.s.ac.GetSuggestions("")
	for idx, s := range m.suggestions {
		m.sl.InsertItem(idx, s.scriptName, "", 0, nil)
	}
	if len(m.suggestions) != 0 {
		m.dt.SetText(m.suggestions[0].desc)
	}

	// Wire up the components.
	m.sl.SetChangedFunc(func(i int, scriptName string, s string, r rune) {
		if i < len(m.suggestions) {
			m.dt.SetText(m.suggestions[i].desc)
		}
	})

	m.sl.SetInputCapture(func(event *tcell.EventKey) *tcell.EventKey {
		switch event.Key() {
		case tcell.KeyEnter:
			scriptName := m.suggestions[m.sl.GetCurrentItem()].scriptName
			m.ib.SetText(scriptName)
			app.SetFocus(m.ib)
			return nil
		case tcell.KeyUp:
			// If you press up and on item zero move up to the input box.
			if m.sl.GetCurrentItem() == 0 {
				app.SetFocus(m.ib)
				return nil
			}
		}
		return event
	})

	m.ib.SetChangedFunc(func(currentText string) {
		m.suggestions = m.s.ac.GetSuggestions(currentText)
		m.sl.Clear()
		for i, s := range m.suggestions {
			sb := strings.Builder{}
			for i := 0; i < len(s.scriptName); i++ {
				if contains(i, s.matchedIndexes) {
					sb.WriteString(fmt.Sprintf("[green]%s[white]", string(s.scriptName[i])))
				} else {
					sb.WriteByte(s.scriptName[i])
				}
			}

			m.sl.InsertItem(i, sb.String(), s.scriptName, 0, nil)
		}
	})

	m.ib.SetInputCapture(func(event *tcell.EventKey) *tcell.EventKey {
		switch event.Key() {
		case tcell.KeyDown:
			app.SetFocus(m.sl)
			return nil
		case tcell.KeyEnter:
			scriptName := stripColors(m.ib.GetText())
			// Check if it's a valid script.
			execScript, err := m.s.br.GetScript(scriptName)
			if err != nil {
				return event
			}
			if m.scriptExecFunc != nil {
				m.scriptExecFunc(execScript)
			}
		case tcell.KeyTAB:
			if len(m.suggestions) == 1 {
				scriptName := m.suggestions[0].scriptName
				m.ib.SetText(stripColors(scriptName))
			} else {
				app.SetFocus(m.sl)
			}
		}

		return event
	})

	app.SetFocus(m.ib)
	return m.layout
}

func (m *autocompleteModal) setScriptExecFunc(f func(s *script.ExecutableScript)) {
	m.scriptExecFunc = f
}

// Close is called when the modal is closed. Nothing to do for autocomplete.
func (m *autocompleteModal) Close(app *tview.Application) {}

type fuzzyAutocompleter struct {
	br *script.BundleManager
	// We cache the script names to make it easier to do searches.
	scriptNames []string
}

func newFuzzyAutoCompleter(br *script.BundleManager) *fuzzyAutocompleter {
	mdList := br.GetOrderedScriptMetadata()
	scriptNames := make([]string, len(mdList))
	for idx, md := range mdList {
		scriptNames[idx] = md.ScriptName
	}

	return &fuzzyAutocompleter{
		br:          br,
		scriptNames: scriptNames,
	}
}

// GetSuggestions returns a list of suggestions.
func (f *fuzzyAutocompleter) GetSuggestions(input string) []suggestion {
	// If the input is empty return all possible values.
	if len(input) == 0 {
		suggestions := make([]suggestion, len(f.scriptNames))
		for i, sn := range f.scriptNames {
			suggestions[i] = suggestion{
				scriptName:     sn,
				desc:           f.br.MustGetScript(sn).Metadata().LongDoc,
				matchedIndexes: nil,
			}
		}
		return suggestions
	}

	matches := fuzzy.Find(input, f.scriptNames)
	suggestions := make([]suggestion, len(matches))
	for i, m := range matches {
		suggestions[i] = suggestion{
			scriptName:     m.Str,
			desc:           f.br.MustGetScript(m.Str).Metadata().LongDoc,
			matchedIndexes: m.MatchedIndexes,
		}
	}
	return suggestions
}
