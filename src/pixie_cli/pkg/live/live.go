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
	"errors"
	"fmt"
	"regexp"
	"sort"
	"strconv"
	"strings"
	"time"
	"unicode"

	"github.com/alecthomas/chroma/quick"
	"github.com/gdamore/tcell"
	"github.com/gofrs/uuid"
	"github.com/rivo/tview"

	apiutils "px.dev/pixie/src/api/go/pxapi/utils"
	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/utils/script"
)

const (
	debugShowBorders = false
	maxCellSize      = 50
	logoColor        = "#3FE7E7"
	textColor        = "#ffffff"
	accentColor      = "#008B8B"
)

type modalType int

const (
	modalTypeUnknown modalType = iota
	modalTypeHelp
	modalTypeAutocomplete
)

var (
	errMissingScript = errors.New("No script provided")
)

type sortType int

const (
	stUnsorted = iota
	stAsc
	stDesc
)

// appState is the global state that is used by the live view.
type appState struct {
	br *script.BundleManager
	ac autocompleter

	viziers []*vizier.Connector
	// The last script that was executed. If nil, nothing was executed.
	execScript *script.ExecutableScript
	// The view of all the tables in the current execution.
	tables          []components.TableView
	tableFormatters []vizier.DataFormatter
	// Sort state is tracked on a per table basis for each column. It is cleared when a new
	// script is executed.
	sortState [][]sortType
	// ----- View Specific State ------
	// The currently selected table. Will reset to zero when new tables are inserted.
	selectedTable int

	scriptViewOpen bool

	// State for search input box.
	searchBoxEnabled bool
	searchEnterHit   bool
	searchString     string
}

// View is the top level of the Live View.
type View struct {
	app               *tview.Application
	pages             *tview.Pages
	tableSelector     *tview.TextView
	infoView          *tview.TextView
	tvTable           *tview.Table
	logoBox           *tview.TextView
	bottomBar         *tview.Flex
	searchBox         *tview.InputField
	modal             Modal
	s                 *appState
	useNewAC          bool
	cloudAddr         string
	selectedClusterID uuid.UUID
	vizierLister      *vizier.Lister
}

// Modal is the interface for a pop-up view.
type Modal interface {
	Show(a *tview.Application) tview.Primitive
	Close(a *tview.Application)
}

// New creates a new live view.
func New(br *script.BundleManager, viziers []*vizier.Connector, cloudAddr string, aClient cloudpb.AutocompleteServiceClient,
	execScript *script.ExecutableScript, useNewAC, useEncryption bool, clusterID uuid.UUID) (*View, error) {
	// App is the top level view. The layout is approximately as follows:
	//  ------------------------------------------
	//  | View Information ...                   |
	//  |________________________________________|
	//  | The actual tables                      |
	//  |                                        |
	//  |                                        |
	//  |                                        |
	//  |________________________________________|
	//  | Table Selector                | Logo   |
	//  ------------------------------------------

	// Top of page.
	infoView := tview.NewTextView()
	infoView.
		SetScrollable(false).
		SetDynamicColors(true).
		SetBorder(debugShowBorders)
	infoView.SetBorderPadding(1, 0, 0, 0)

	topBar := tview.NewFlex().
		SetDirection(tview.FlexColumn).
		AddItem(infoView, 0, 50, true)

	// Middle of page.
	pages := tview.NewPages()
	pages.SetBorder(debugShowBorders)

	// Bottom of Page.
	logoBox := tview.NewTextView().
		SetScrollable(false).
		SetDynamicColors(true)

	// Print out the logo.
	fmt.Fprintf(logoBox, "\n  [%s]PIXIE[%s]", logoColor, textColor)

	tableSelector := tview.NewTextView()
	bottomBar := tview.NewFlex().
		SetDirection(tview.FlexColumn).
		AddItem(tableSelector, 0, 1, false).
		AddItem(logoBox, 8, 1, false)
	bottomBar.SetBorderPadding(1, 0, 0, 0)

	layout := tview.NewFlex().
		SetDirection(tview.FlexRow).
		AddItem(topBar, 3, 0, false).
		AddItem(pages, 0, 1, true).
		AddItem(bottomBar, 2, 0, false)

	searchBox := tview.NewInputField()
	searchBox.SetBackgroundColor(tcell.ColorBlack)
	searchBox.SetFieldBackgroundColor(tcell.ColorBlack)

	// Application setup.
	app := tview.NewApplication()
	app.SetRoot(layout, true).
		EnableMouse(true)

	var ac autocompleter
	if useNewAC {
		ac = newCloudAutocompleter(aClient)
	} else {
		ac = newFuzzyAutoCompleter(br)
	}

	lister, err := vizier.NewLister(cloudAddr)
	if err != nil {
		utils.WithError(err).Error("Failed to create Vizier lister")
		return nil, err
	}

	v := &View{
		app:           app,
		pages:         pages,
		tableSelector: tableSelector,
		infoView:      infoView,
		logoBox:       logoBox,
		searchBox:     searchBox,
		bottomBar:     bottomBar,
		s: &appState{
			br:         br,
			viziers:    viziers,
			ac:         ac,
			execScript: execScript,
		},
		useNewAC:          useNewAC,
		cloudAddr:         cloudAddr,
		selectedClusterID: clusterID,
		vizierLister:      lister,
	}

	// Wire up components.
	tableSelector.
		SetDynamicColors(true).
		SetRegions(true).
		SetWrap(false)

	// When table selector is highlighted (ie. mouse click or number). We use the region
	// to select the appropriate table.
	tableSelector.SetHighlightedFunc(func(added, removed, remaining []string) {
		if len(added) > 0 {
			if tableNum, err := strconv.Atoi(added[0]); err == nil {
				v.selectTable(tableNum)
			}
		}
	})

	searchBox.SetChangedFunc(v.search)
	searchBox.SetInputCapture(v.searchInputCapture)
	// If a default script was passed in execute it.
	v.runScript(execScript, useEncryption)

	// Wire up the main keyboard handler.
	app.SetInputCapture(v.keyHandler)
	return v, nil
}

// Run runs the view.
func (v *View) Run() error {
	return v.app.Run()
}

// Stop stops the view and kills the app.
func (v *View) Stop() {
	v.app.Stop()
}

// runScript is the internal method to run an executable script and update relevant appState.
func (v *View) runScript(execScript *script.ExecutableScript, useEncryption bool) {
	v.clearErrorIfAny()
	if execScript == nil {
		v.execCompleteWithError(errMissingScript)
		return
	}
	v.s.execScript = execScript
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	var encOpts, decOpts *vizierpb.ExecuteScriptRequest_EncryptionOptions
	var err error
	if useEncryption {
		encOpts, decOpts, err = apiutils.CreateEncryptionOptions()
		if err != nil {
			v.execCompleteWithError(err)
			return
		}
	}

	resp, err := vizier.RunScript(ctx, v.s.viziers, execScript, encOpts)
	if err != nil {
		v.execCompleteWithError(err)
		return
	}
	tw := vizier.NewStreamOutputAdapter(ctx, resp, vizier.FormatInMemory, decOpts)
	err = tw.Finish()
	if err != nil {
		v.execCompleteWithError(err)
		return
	}

	v.s.tables, err = tw.Views()
	if err != nil {
		v.execCompleteWithError(err)
		return
	}

	v.s.tableFormatters, err = tw.Formatters()
	if err != nil {
		v.execCompleteWithError(err)
		return
	}

	// Reset sort state.
	v.s.sortState = make([][]sortType, len(v.s.tables))
	for i, t := range v.s.tables {
		// Default value is unsorted.
		v.s.sortState[i] = make([]sortType, len(t.Header()))
	}
	// The view can update with nil data if there is an error.
	v.s.selectedTable = 0

	v.execCompleteViewUpdate()
}

func (v *View) clearErrorIfAny() {
	// Clear error pages if any.
	if v.pages.HasPage("error") {
		v.pages.RemovePage("error")
	}
}
func (v *View) execCompleteWithError(err error) {
	v.searchClear()
	v.closeModal()

	fmt.Print(err.Error())
	var m string
	if v.s.execScript == nil {
		m = "No Script Provided.\n"
		m += "Type '?' for help or ctrl-k to get started."
	} else {
		m = vizier.FormatErrorMessage(err)
	}
	tv := tview.NewTextView()
	tv.SetDynamicColors(true)
	tv.SetText(tview.TranslateANSI(m))

	v.s.selectedTable = 0
	v.tvTable = nil
	v.pages.AddAndSwitchToPage("error", tv, true)
	v.app.SetFocus(tv)
}

func (v *View) execCompleteViewUpdate() {
	v.closeModal()
	v.searchClear()

	v.updateScriptInfoView()
	v.updateTableNav()
	v.renderCurrentTable()
}

func (v *View) updateScriptInfoView() {
	v.infoView.Clear()

	// Get the name for this cluster for the live view
	var clusterName *string
	vzInfo, err := v.vizierLister.GetVizierInfo(v.selectedClusterID)
	switch {
	case err != nil:
		utils.WithError(err).Errorf("Error getting cluster name for cluster %s", v.selectedClusterID.String())
	case len(vzInfo) == 0:
		utils.Errorf("Error getting cluster name for cluster %s, no results returned", v.selectedClusterID.String())
	default:
		clusterName = &(vzInfo[0].ClusterName)
	}

	fmt.Fprintf(v.infoView, "%s : %s", withAccent("Script"),
		v.s.execScript.ScriptName)
	args := v.s.execScript.Args
	if len(args) > 0 {
		for _, arg := range args {
			fmt.Fprintf(v.infoView, " --%s=%s ", withAccent(arg.Name), arg.Value)
		}
	}

	fmt.Fprintf(v.infoView, "\n")
	if lvl := v.s.execScript.LiveViewLink(clusterName); lvl != "" {
		fmt.Fprintf(v.infoView, "%s %s", withAccent("Live View:"), lvl)
	}
}

func (v *View) renderCurrentTable() {
	// We remove all the old pages and create new pages for tables.
	if v.pages.HasPage("table") {
		v.pages.RemovePage("table")
	}

	if len(v.s.tables) < v.s.selectedTable {
		return
	}
	table := v.s.tables[v.s.selectedTable]
	formatter := v.s.tableFormatters[v.s.selectedTable]
	v.tvTable = v.createTviewTable(table, formatter, v.s.sortState[v.s.selectedTable])
	v.pages.AddAndSwitchToPage("table", v.tvTable, true)
	v.app.SetFocus(v.pages)
}

func (v *View) updateTableNav() {
	v.tableSelector.Clear()
	for idx, t := range v.s.tables {
		fmt.Fprintf(v.tableSelector, `%d ["%d"]%s[""]  `, idx+1, idx, withAccent(t.Name()))
	}
	v.showTableNav()
}

func (v *View) selectNextTable() {
	v.selectTableAndHighlight(v.s.selectedTable + 1)
}

func (v *View) selectPrevTable() {
	v.selectTableAndHighlight(v.s.selectedTable - 1)
}

func (v *View) createTviewTable(t components.TableView, formatter vizier.DataFormatter, sortState []sortType) *tview.Table {
	table := tview.NewTable().
		SetBorders(true).
		SetSelectable(true, true).
		SetFixed(1, 0)

	for idx, val := range t.Header() {
		// Render the header.
		tableCell := tview.NewTableCell(withAccent(val) + sortIcon(sortState[idx])).
			SetAlign(tview.AlignCenter).
			SetSelectable(false).
			SetExpansion(2)
		table.SetCell(0, idx, tableCell)
	}

	data := t.Data()
	// Sort columns from left to right.
	sorting := false
	for _, order := range sortState {
		if order != stUnsorted {
			sorting = true
		}
	}
	if sorting {
		for idx, order := range sortState {
			if order == stUnsorted {
				continue
			}
			sort.SliceStable(data, func(i, j int) bool {
				return colCompare(data[i][idx], data[j][idx], order)
			})
		}
	}

	for rowIdx, row := range data {
		for colIdx, val := range row {
			s := formatter.FormatValue(colIdx, val).(string)
			if len(s) > maxCellSize {
				s = s[:maxCellSize-1] + "\u2026"
			}
			tableCell := tview.NewTableCell(tview.TranslateANSI(s)).
				SetTextColor(tcell.ColorWhite).
				SetAlign(tview.AlignLeft).
				SetSelectable(true).
				SetExpansion(2)
			table.SetCell(rowIdx+1, colIdx, tableCell)
		}
	}

	handleLargeBlobView := func(row, column int) {
		v.closeModal()

		if row < 1 || column < 0 {
			return
		}

		// Try to parse large blob as a string, we only know how to render large strings
		// so bail if we can't convert to string or if it's not that big.
		d := t.Data()[row-1][column]
		s, ok := d.(string)
		if !ok || len(s) < maxCellSize {
			return
		}

		renderString := tryJSONHighlight(s)
		v.showDataModal(tview.TranslateANSI(renderString))
	}

	// Since selection and mouse clicks happen in two different events, we need to track the selection
	// rows/cols in variables so that we can show the right popup.
	selectedRow := 0
	selectedCol := 0
	table.SetMouseCapture(func(action tview.MouseAction, event *tcell.EventMouse) (tview.MouseAction, *tcell.EventMouse) {
		if action == tview.MouseLeftDoubleClick {
			handleLargeBlobView(selectedRow, selectedCol)
			// For some reason the double click event does not trigger a redraw.
			v.app.ForceDraw()
			return action, event
		}
		return action, event
	})

	table.SetSelectionChangedFunc(func(row, column int) {
		//fmt.Printf("%+v  %+v\n", row, column)
		// Switch the sort state.
		if row == 0 {
			cs := v.s.sortState[v.s.selectedTable][column]
			v.s.sortState[v.s.selectedTable][column] = nextSort(cs)
			v.renderCurrentTable()
		}
		// Store the selection so we can pop open the blob view on double click.
		selectedRow = row
		selectedCol = column
		// This function is triggered when mouse is used after modal is open, in which case we can switch the blob.
		if v.modal != nil {
			handleLargeBlobView(row, column)
		}
	})

	table.SetSelectedFunc(handleLargeBlobView)

	return table
}

func (v *View) showScriptView() {
	v.s.scriptViewOpen = true
	tv := tview.NewTextView()
	tv.SetDynamicColors(true)
	v.pages.AddAndSwitchToPage("script", tv, true)
	if v.s.execScript != nil {
		highlighted := strings.Builder{}
		quick.Highlight(&highlighted, v.s.execScript.ScriptString, "python", //nolint: errcheck
			"terminal16m", "monokai")
		fmt.Fprintf(tv, "%s :\n\n", withAccent("Script View"))
		fmt.Fprint(tv, tview.TranslateANSI(highlighted.String()))
	} else {
		fmt.Fprintf(tv, "[red]Script Not Found[white]")
	}

	v.app.SetFocus(tv)
}

func (v *View) closeScriptView() {
	if !v.s.scriptViewOpen {
		return
	}
	v.pages.RemovePage("script")
	v.s.scriptViewOpen = false
	v.selectTableAndHighlight(v.s.selectedTable)
}

func (v *View) showDataModal(s string) {
	v.closeModal()
	d := newDetailsModal(s)
	m := d.Show(v.app)
	v.pages.AddPage("modal", createModal(m, 60, 30), true, true)
	v.modal = d
}

func (v *View) showAutcompleteModal() {
	v.closeModal()
	var ac AutocompleteModal
	if v.useNewAC {
		ac = newTabAutocompleteModal(v.s)
	} else {
		ac = newAutocompleteModal(v.s)
	}
	ac.SetScriptExecFunc(func(s *script.ExecutableScript) {
		v.runScript(s, true)
	})
	v.modal = ac
	v.pages.AddPage("modal", createModal(v.modal.Show(v.app),
		65, 30), true, true)
}

func (v *View) showHelpModal() {
	v.closeModal()
	hm := &helpModal{}
	v.modal = hm
	v.pages.AddPage("modal", createModal(v.modal.Show(v.app),
		65, 30), true, true)
}

// closes modal if open, noop if not.
func (v *View) closeModal() {
	if v.modal == nil {
		return
	}
	v.pages.RemovePage("modal")
	v.modal = nil
	if v.s.searchBoxEnabled {
		// This will refocus the search box.
		v.showSearchBox()
	} else {
		// This will cause a refocus to occur on the table.
		v.selectTableAndHighlight(v.s.selectedTable)
	}
}

// selectTableAndHighlight selects and highligts the table. Don't call this from within the highlight func
// or you will get an infinite loop.
func (v *View) selectTableAndHighlight(tableNum int) {
	tableNum = v.selectTable(tableNum)
	v.tableSelector.Highlight(strconv.Itoa(tableNum)).ScrollToHighlight()
}

func (v *View) showTableNav() {
	v.s.searchBoxEnabled = false
	// Clear the text box.
	v.searchClear()
	v.bottomBar.
		Clear().
		AddItem(v.tableSelector, 0, 1, false).
		AddItem(v.logoBox, 8, 1, false)

	// Switch focus back to the active table.
	v.selectTableAndHighlight(v.s.selectedTable)
}

func (v *View) showSearchBox() {
	v.s.searchBoxEnabled = true
	v.bottomBar.
		Clear().
		AddItem(v.searchBox, 0, 1, false).
		AddItem(v.logoBox, 8, 1, false)
	v.app.SetFocus(v.searchBox)
}

// selectTable selects the numbered table. Out of bounds wrap in both directions.
func (v *View) selectTable(tableNum int) int {
	if v.s.scriptViewOpen {
		v.closeScriptView()
	}
	if len(v.s.tables) == 0 {
		return 0
	}
	tableNum %= len(v.s.tables)

	// We only need to render if it's a different table.
	if v.s.selectedTable != tableNum {
		v.s.selectedTable = tableNum
		v.renderCurrentTable()
	}
	v.app.SetFocus(v.pages)

	return tableNum
}

func (v *View) activeModalType() modalType {
	if v.modal == nil {
		return modalTypeUnknown
	}
	switch v.modal.(type) {
	case *helpModal:
		return modalTypeHelp
	case *autocompleteModal:
		return modalTypeAutocomplete
	default:
		return modalTypeUnknown
	}
}

func (v *View) searchClear() {
	v.s.searchEnterHit = false
	v.s.searchString = ""
	v.searchBox.SetText("")
}

func (v *View) search(s string) {
	v.s.searchString = s
	v.searchNext(false, false)
	v.app.SetFocus(v.searchBox)
}

func (v *View) searchNext(searchBackwards bool, advance bool) {
	s := v.s.searchString
	if s == "" {
		return
	}

	// Very unoptimized search function...
	if v.tvTable == nil {
		return
	}
	t := v.tvTable
	rc := t.GetRowCount()
	cc := t.GetColumnCount()

	searchFunc := func(t string) bool {
		return strings.Contains(t, s)
	}

	// If possible try to make it a regexp.
	re, err := regexp.Compile(s)
	if err == nil {
		searchFunc = func(t string) bool {
			return re.Match([]byte(t))
		}
	}
	wrappedCount := 0
	for wrappedCount < 2 {
		r, c := t.GetSelection()
		if advance {
			c++
		}
		rowCond := func() bool {
			return r < rc
		}
		colCond := func() bool {
			return c < cc
		}

		if searchBackwards {
			if advance {
				c -= 2 // For the increment above, and back one more col.
			}
			rowCond = func() bool {
				return r >= 0
			}
			colCond = func() bool {
				return c >= 0
			}
		}

		for rowCond() {
			for colCond() {
				if searchFunc(stripColors(t.GetCell(r, c).Text)) {
					t.Select(r, c)
					return
				}
				if searchBackwards {
					c--
				} else {
					c++
				}
			}
			if searchBackwards {
				c = cc
				r--
			} else {
				c = 0
				r++
			}
		}

		// Roll over.
		if searchBackwards {
			t.Select(rc, cc)
		} else {
			t.Select(0, 0)
		}
		wrappedCount++
	}
}

func (v *View) searchInputCapture(event *tcell.EventKey) *tcell.EventKey {
	switch event.Key() {
	case tcell.KeyEnter:
		v.s.searchEnterHit = true
		return nil
	case tcell.KeyBackspace2:
		fallthrough
	case tcell.KeyBackspace:
		fallthrough
	case tcell.KeyDelete:
		v.s.searchEnterHit = false
		return event
	case tcell.KeyCtrlR:
		v.searchNext(true, true)
		return nil
	case tcell.KeyCtrlS:
		v.searchNext(false, true)
		return nil
	case tcell.KeyRune:
		if v.s.searchEnterHit {
			s := string(event.Rune())
			if s == "n" {
				v.searchNext(false, true)
			}
			if s == "p" {
				v.searchNext(true, true)
			}
			return nil
		}
	case tcell.KeyCtrlG:
		fallthrough
	case tcell.KeyEscape:
		v.searchClear()
		v.showTableNav()
		return nil
	}
	return event
}

func (v *View) keyHandler(event *tcell.EventKey) *tcell.EventKey {
	// If the modal is open capture the event and let escape or the original
	// shortcut close it.
	if v.modal != nil {
		switch event.Key() {
		case tcell.KeyEscape:
			v.closeModal()
			return nil
		case tcell.KeyRune:
			if string(event.Rune()) == "?" {
				if v.activeModalType() == modalTypeHelp {
					v.closeModal()
					return nil
				}
			}
		case tcell.KeyCtrlK:
			if v.activeModalType() == modalTypeAutocomplete {
				v.closeModal()
				return nil
			}
		}
		return event
	}

	if v.s.searchBoxEnabled {
		if event.Key() == tcell.KeyCtrlK {
			v.showTableNav()
			v.searchClear()
			v.showAutcompleteModal()
		}
		return event
	}

	switch event.Key() {
	case tcell.KeyTAB:
		// Default for tab is to quit so stop that.
		return nil
	case tcell.KeyCtrlN:
		v.selectNextTable()
	case tcell.KeyCtrlP:
		v.selectPrevTable()
	case tcell.KeyRune:
		// Switch to a specific view. This will be a no-op if no tables are loaded.
		r := event.Rune()
		if unicode.IsDigit(r) {
			v.selectTableAndHighlight(int(r-'0') - 1)
		}

		if string(r) == "?" {
			v.showHelpModal()
			return nil
		}
		if string(r) == "/" {
			v.showSearchBox()
			return nil
		}
	case tcell.KeyCtrlS:
		v.showSearchBox()
		return nil
	case tcell.KeyCtrlV:
		if v.s.scriptViewOpen {
			v.closeScriptView()
			return nil
		}
		v.showScriptView()
		return nil
	case tcell.KeyCtrlK:
		v.showAutcompleteModal()
		return nil
	case tcell.KeyCtrlR:
		v.runScript(v.s.execScript, true)
		return nil
	}

	// Ctrl-c, etc. can happen based on default handlers.
	return event
}

func sortIcon(s sortType) string {
	switch s {
	case stUnsorted:
		return " \u2195"
	case stAsc:
		return " \u2191"
	case stDesc:
		return " \u2193"
	}
	return ""
}

func nextSort(s sortType) sortType {
	switch s {
	case stUnsorted:
		return stAsc
	case stAsc:
		return stDesc
	case stDesc:
		fallthrough
	default:
		return stUnsorted
	}
}

func colCompare(v1 interface{}, v2 interface{}, s sortType) bool {
	switch v1c := v1.(type) {
	case time.Time:
		v2c, ok := v2.(time.Time)
		if !ok {
			break
		}
		if s == stAsc {
			return v1c.Sub(v2c) > 0
		}
		return v1c.Sub(v2c) < 0
	case float64:
		v2c, ok := v2.(float64)
		if !ok {
			break
		}
		if s == stAsc {
			return v1c < v2c
		}
		return v2c < v1c
	case int64:
		v2c, ok := v2.(int64)
		if !ok {
			break
		}
		if s == stAsc {
			return v1c < v2c
		}
		return v2c < v1c
	}

	// Sort as strings, since we can't find type.
	v1c := fmt.Sprintf("%+v", v1)
	v2c := fmt.Sprintf("%+v", v2)
	if s == stAsc {
		return v1c < v2c
	}
	return v2c < v1c
}
