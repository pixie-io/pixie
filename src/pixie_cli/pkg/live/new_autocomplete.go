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

package live

import (
	"context"
	"fmt"
	"strings"

	"github.com/gdamore/tcell"
	"github.com/rivo/tview"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/utils/script"
)

// AutocompleteModal is a modal for autocomplete.
type AutocompleteModal interface {
	Modal
	SetScriptExecFunc(func(s *script.ExecutableScript))
}

var protoToKindLabelMap = map[cloudpb.AutocompleteEntityKind]string{
	cloudpb.AEK_SVC:       "svc",
	cloudpb.AEK_POD:       "pod",
	cloudpb.AEK_SCRIPT:    "script",
	cloudpb.AEK_NAMESPACE: "ns",
}

type suggestion struct {
	name string
	desc string
	kind cloudpb.AutocompleteEntityKind

	matchedIndexes []int
}

type autocompleter interface {
	GetSuggestions(string, int, cloudpb.AutocompleteActionType) ([]*TabStop, map[int][]suggestion, bool, error)
}

// autcompleteModal is the autocomplete modal.
type tabAutocompleteModal struct {
	// Reference to the parent view.
	s *appState

	// The input box.
	input *components.InputField

	// The suggestion list.
	sl *tview.List
	// The description text box.
	dt *tview.TextView

	layout *tview.Flex

	// Current input state.
	scriptExecFunc func(*script.ExecutableScript)
	valid          bool // Whether the current input is valid.
	tabStops       []*TabStop
	suggestions    map[int][]suggestion
	tabStopIndex   int                 // The index of the current tab stop.
	boundaries     []tabStopBoundaries // The index positions of where each tabstop begins/ends in the output string.
}

type tabStopBoundaries struct {
	left  int
	right int
}

func newTabAutocompleteModal(st *appState) *tabAutocompleteModal {
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

	inputBox := components.NewInputField()
	inputBox.
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
		AddItem(scriptListBox, 0, 9, false).
		AddItem(scriptDescBox, 0, 10, false)

	layout := tview.NewFlex().
		SetDirection(tview.FlexRow).
		AddItem(inputBox, 3, 0, true).
		AddItem(horiz, 0, 1, false)

	return &tabAutocompleteModal{
		s:      st,
		input:  inputBox,
		sl:     scriptListBox,
		dt:     scriptDescBox,
		layout: layout,
	}
}

func (m *tabAutocompleteModal) updateSuggestions() {
	m.sl.Clear()

	suggestions := m.suggestions[*m.tabStops[m.tabStopIndex].Index]

	for i, s := range suggestions {
		sb := strings.Builder{}
		if l, ok := protoToKindLabelMap[s.kind]; ok {
			sb.WriteString(fmt.Sprintf("[yellow]%s:[white]", l))
		}
		for i := 0; i < len(s.name); i++ {
			if contains(i, s.matchedIndexes) {
				sb.WriteString(fmt.Sprintf("[green]%s[white]", string(s.name[i])))
			} else {
				sb.WriteByte(s.name[i])
			}
		}

		m.sl.InsertItem(i, sb.String(), s.name, 0, nil)
	}

	if len(suggestions) > 0 {
		m.dt.SetText(suggestions[m.sl.GetCurrentItem()].desc)
	} else {
		m.dt.Clear()
	}
}

func (m *tabAutocompleteModal) parseCommand() (*script.ExecutableScript, error) {
	startingIdx := 0

	if *m.tabStops[0].Value == "run" {
		startingIdx++ // Script starts at first index.
	}

	es, err := m.s.br.GetScript(strings.Replace(*m.tabStops[startingIdx].Value, "$0", "", 1))
	if err != nil {
		return nil, err
	}

	if len(m.tabStops) == startingIdx+1 {
		return es, nil
	}

	fs := es.GetFlagSet()
	if fs == nil {
		return es, nil
	}

	for i := startingIdx + 1; i < len(m.tabStops); i++ {
		err = fs.Set(*m.tabStops[i].Label, strings.Replace(*m.tabStops[i].Value, "$0", "", 1))
		if err != nil {
			return nil, err
		}
	}
	err = es.UpdateFlags(fs)
	if err != nil {
		return nil, err
	}
	return es, nil
}

func (m *tabAutocompleteModal) selectSuggestion(app *tview.Application, s suggestion) {
	ts := m.tabStops[m.tabStopIndex]

	kind := s.kind
	value := s.name
	label := ""

	if !ts.HasLabel {
		if l, ok := protoToKindLabelMap[kind]; ok {
			label = l
		}
	} else {
		label = *ts.Label
	}

	// Replace the tabstop in the display string with the suggestion.
	currentString := m.input.GetText()

	newStr := ""
	if m.tabStopIndex > 0 {
		newStr = currentString[:m.boundaries[m.tabStopIndex-1].right] + " "
	}

	if label != "" {
		newStr += label + ":"
	}

	newStr += value

	newCursorPos := len(newStr)

	if len(m.tabStops)-1 > m.tabStopIndex {
		newStr += currentString[m.boundaries[m.tabStopIndex].right:]
	}

	m.updateInput(app, newStr, newCursorPos, cloudpb.AAT_SELECT)
	app.SetFocus(m.input)
}

func (m *tabAutocompleteModal) updateInput(app *tview.Application, text string, inCursorPos int, action cloudpb.AutocompleteActionType) {
	tabStops, suggestions, isExecutable, err := m.s.ac.GetSuggestions(text, inCursorPos, action)
	if err != nil {
		return
	}

	m.valid = isExecutable
	if isExecutable {
		m.input.SetBorderColor(tcell.ColorGreen)
	} else {
		m.input.SetBorderColor(tcell.ColorYellow)
	}

	// Iterate through the tabstops to generate the display string we show to the user.
	// During this parsing, we determine the current cursor position and the indices of the
	// tabstop values in the display string.
	displayStr := ""
	cursorPos := 0
	m.boundaries = make([]tabStopBoundaries, 0)
	textColors := make(map[int]tcell.Color)
	for i, t := range tabStops {
		value := ""
		label := ""

		if t.Value != nil { // Tabstop has a label.
			value = *t.Value
			label = *t.Label
		} else if t.Label != nil {
			if t.HasLabel {
				label = *t.Label
			} else {
				value = *t.Label
			}
		}

		c := strings.Index(label, "$0") // Determine if the cursor is in the label.
		if c != -1 {
			cursorPos = len(displayStr) + c
			label = strings.Replace(label, "$0", "", 1)
			m.tabStopIndex = i
		}

		if label != "" {
			for j := len(displayStr); j < len(displayStr)+len(label)+1; j++ {
				textColors[j] = tcell.ColorYellow
			}
			displayStr += label + ":"
		}
		valueStartIndex := len(displayStr)

		c = strings.Index(value, "$0") // Determine if the cursor is in the value.
		if c != -1 {
			cursorPos = len(displayStr) + c
			value = strings.Replace(value, "$0", "", 1)
			m.tabStopIndex = i
		}

		displayStr += value

		if i != len(tabStops)-1 {
			displayStr += " "
		}

		m.boundaries = append(m.boundaries, tabStopBoundaries{valueStartIndex, len(displayStr) - 1})
	}

	m.input.SetText(displayStr, textColors, false)
	m.input.SetCursorPos(cursorPos)

	m.tabStops = tabStops
	m.suggestions = suggestions

	m.updateSuggestions()
}

// handleBackspace determines whether the next item is a label or value and modifies the string accordingly.
func (m *tabAutocompleteModal) handleBackspace(app *tview.Application) bool {
	curText := m.input.GetText()
	currBoundaries := m.boundaries[m.tabStopIndex]
	newCursor := 0
	if m.tabStopIndex > 0 {
		m.tabStopIndex--
		newCursor = m.boundaries[m.tabStopIndex].right
	}
	if len(curText) > 0 && string(curText[m.input.GetCursorPos()-1]) == ":" { // User is deleting a label.
		m.updateInput(app, m.input.GetText()[:currBoundaries.left]+" "+m.input.GetText()[currBoundaries.right+1:], newCursor, cloudpb.AAT_EDIT)
		return true
	}
	return false
}

// Show shows the modal.
func (m *tabAutocompleteModal) Show(app *tview.Application) tview.Primitive {
	// Start with suggestions based on empty input.
	app.SetFocus(m.input)
	m.updateInput(app, "", m.input.GetCursorPos(), cloudpb.AAT_EDIT)

	m.input.SetChangedFunc(func(currentText string) {
		m.updateInput(app, currentText, m.input.GetCursorPos(), cloudpb.AAT_EDIT)
	})

	m.input.SetInputCapture(func(event *tcell.EventKey) *tcell.EventKey {
		switch event.Key() {
		case tcell.KeyLeft:
			if len(m.boundaries) > 0 {
				currBoundaries := m.boundaries[m.tabStopIndex]
				curPos := m.input.GetCursorPos() - 1
				if curPos >= 0 && curPos < currBoundaries.left && m.tabStopIndex > 0 {
					// The cursor is past the current tabstop's boundary. We should move to the previous tab stop,
					// skipping this current tabstop's label, if any.
					m.tabStopIndex--
					newCursorPos := m.boundaries[m.tabStopIndex].right
					m.input.SetCursorPos(newCursorPos)
					m.updateSuggestions()
				} else if m.tabStopIndex == 0 && curPos < currBoundaries.left {
					return nil // Don't change the cursor position.
				}
			}
		case tcell.KeyRight:
			if len(m.boundaries) > 0 {
				currBoundaries := m.boundaries[m.tabStopIndex]
				curPos := m.input.GetCursorPos() + 1
				if curPos <= len(m.input.GetText()) && curPos > currBoundaries.right && m.tabStopIndex < len(m.tabStops)-1 {
					// The cursor is past the current tabstop's boundary. We should move to the next tab stop,
					// skipping that tabstop's label, if any.
					m.tabStopIndex++
					newCursorPos := m.boundaries[m.tabStopIndex].left
					m.input.SetCursorPos(newCursorPos)
					m.updateSuggestions()
				}
			}
		case tcell.KeyBackspace:
			shouldReturn := m.handleBackspace(app)
			if shouldReturn {
				return nil
			}
		case tcell.KeyBackspace2:
			shouldReturn := m.handleBackspace(app)
			if shouldReturn {
				return nil
			}
		case tcell.KeyDown:
			app.SetFocus(m.sl)
			return nil
		case tcell.KeyEnter:
			if !m.valid {
				return event
			}

			execScript, err := m.parseCommand()
			if err != nil {
				return event
			}
			if m.scriptExecFunc != nil {
				m.scriptExecFunc(execScript)
			}
		case tcell.KeyTAB:
			suggestions := m.suggestions[*m.tabStops[m.tabStopIndex].Index]
			if len(suggestions) == 1 {
				m.selectSuggestion(app, suggestions[0])
				return nil
			}
			app.SetFocus(m.sl)
		}

		return event
	})

	// Wire up the components.
	m.sl.SetChangedFunc(func(i int, scriptName string, s string, r rune) {
		if val, ok := m.suggestions[*m.tabStops[m.tabStopIndex].Index]; ok {
			if i < len(val) {
				m.dt.SetText(val[i].desc)
			}
		}
	})

	m.sl.SetInputCapture(func(event *tcell.EventKey) *tcell.EventKey {
		ts := m.tabStops[m.tabStopIndex]
		if val, ok := m.suggestions[*ts.Index]; ok {
			switch event.Key() {
			case tcell.KeyEnter:
				m.selectSuggestion(app, val[m.sl.GetCurrentItem()])
				return nil
			case tcell.KeyUp:
				// If you press up and on item zero move up to the input box.
				if m.sl.GetCurrentItem() == 0 {
					app.SetFocus(m.input)
					return nil
				}
			}
		}
		return event
	})

	return m.layout
}

// SetScriptExecFunc sets the script exec func for the modal.
func (m *tabAutocompleteModal) SetScriptExecFunc(f func(s *script.ExecutableScript)) {
	m.scriptExecFunc = f
}

// Close is called when the modal is closed. Nothing to do for autocomplete.
func (m *tabAutocompleteModal) Close(app *tview.Application) {}

type cloudAutocompleter struct {
	client cloudpb.AutocompleteServiceClient
}

func newCloudAutocompleter(client cloudpb.AutocompleteServiceClient) *cloudAutocompleter {
	return &cloudAutocompleter{
		client,
	}
}

// GetSuggestions returns the tabstops for the given input string, action, and cursor position.
func (a *cloudAutocompleter) GetSuggestions(input string, cursor int, action cloudpb.AutocompleteActionType) ([]*TabStop, map[int][]suggestion, bool, error) {
	req := &cloudpb.AutocompleteRequest{
		Input:     input,
		CursorPos: int64(cursor),
		Action:    action,
	}

	ctxWithCreds := auth.CtxWithCreds(context.Background())
	resp, err := a.client.Autocomplete(ctxWithCreds, req)
	if err != nil {
		return nil, nil, false, err
	}

	// Format results.
	cmd, err := ParseInput(resp.FormattedInput)
	if err != nil {
		return nil, nil, false, err
	}

	suggestionsMap := make(map[int][]suggestion)
	for _, s := range resp.TabSuggestions {
		suggestionsMap[int(s.TabIndex)] = make([]suggestion, len(s.Suggestions))
		for i, v := range s.Suggestions {
			matchedIdxs := make([]int, len(v.MatchedIndexes))
			for i, matched := range v.MatchedIndexes {
				matchedIdxs[i] = int(matched)
			}
			suggestionsMap[int(s.TabIndex)][i] = suggestion{
				name:           v.Name,
				desc:           v.Description,
				kind:           v.Kind,
				matchedIndexes: matchedIdxs,
			}
		}
	}
	return cmd.TabStops, suggestionsMap, resp.IsExecutable, nil
}
