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
	"math"
	"regexp"

	"github.com/gdamore/tcell"
	"github.com/mattn/go-runewidth"
	"github.com/rivo/tview"
	"github.com/rivo/uniseg"
)

// InputField is a one-line box which scrolls.
// This is copied from https://github.com/rivo/tview/blob/e352ec01560d2efc19918e4732310255b2bb800a/inputfield.go
// and modified to expose cursor position and more custom event handling.
// The following keys can be used for navigation and editing:
//
//   - Left arrow: Move left by one character.
//   - Right arrow: Move right by one character.
//   - Home, Ctrl-A, Alt-a: Move to the beginning of the line.
//   - End, Ctrl-E, Alt-e: Move to the end of the line.
//   - Alt-left, Alt-b: Move left by one word.
//   - Alt-right, Alt-f: Move right by one word.
//   - Backspace: Delete the character before the cursor.
//   - Delete: Delete the character after the cursor.
//   - Ctrl-K: Delete from the cursor to the end of the line.
//   - Ctrl-W: Delete the last word before the cursor.
//   - Ctrl-U: Delete the entire line.
type InputField struct {
	*tview.Box
	hasFocus bool
	// The text that was entered.
	text string

	// The background color of the input area.
	fieldBackgroundColor tcell.Color

	// The text color of the input area.
	fieldTextColor tcell.Color

	// The text color of the placeholder.
	placeholderTextColor tcell.Color

	// The cursor position as a byte index into the text string.
	cursorPos int

	// An optional function which is called when the input has changed.
	changed func(text string)

	fieldX int // The x-coordinate of the input field as determined during the last call to Draw().
	offset int // The number of bytes of the text string skipped ahead while drawing.

	textColors map[int]tcell.Color // The colors of the text, by index.
}

// NewInputField returns a new input field.
func NewInputField() *InputField {
	return &InputField{
		Box:                  tview.NewBox(),
		fieldBackgroundColor: tcell.ColorBlack,
		fieldTextColor:       tview.Styles.PrimaryTextColor,
		placeholderTextColor: tview.Styles.ContrastSecondaryTextColor,
	}
}

// SetText sets the current text of the input field.
func (i *InputField) SetText(text string, textColors map[int]tcell.Color, triggerChange bool) *InputField {
	i.text = text
	i.cursorPos = len(text)
	i.textColors = textColors
	if i.changed != nil && triggerChange {
		i.changed(text)
	}
	return i
}

// GetText returns the current text of the input field.
func (i *InputField) GetText() string {
	return i.text
}

// SetFieldBackgroundColor sets the background color of the input area.
func (i *InputField) SetFieldBackgroundColor(color tcell.Color) *InputField {
	i.fieldBackgroundColor = color
	return i
}

// SetFieldTextColor sets the text color of the input area.
func (i *InputField) SetFieldTextColor(color tcell.Color) *InputField {
	i.fieldTextColor = color
	return i
}

// SetPlaceholderTextColor sets the text color of placeholder text.
func (i *InputField) SetPlaceholderTextColor(color tcell.Color) *InputField {
	i.placeholderTextColor = color
	return i
}

// SetChangedFunc sets a handler which is called whenever the text of the input
// field has changed. It receives the current text (after the change).
func (i *InputField) SetChangedFunc(handler func(text string)) *InputField {
	i.changed = handler
	return i
}

// GetCursorPos gets the current cursor position.
func (i *InputField) GetCursorPos() int {
	return i.cursorPos
}

// SetCursorPos sets the current cursor position.
func (i *InputField) SetCursorPos(p int) {
	i.cursorPos = p
}

// Draw draws this primitive onto the screen.
func (i *InputField) Draw(screen tcell.Screen) {
	i.Box.Draw(screen)

	// Prepare
	x, y, width, height := i.GetInnerRect()
	rightLimit := x + width
	if height < 1 || rightLimit <= x {
		return
	}

	// Draw input area.
	i.fieldX = x
	fieldWidth := math.MaxInt32

	if rightLimit-x < fieldWidth {
		fieldWidth = rightLimit - x
	}
	fieldStyle := tcell.StyleDefault.Background(i.fieldBackgroundColor)
	for index := 0; index < fieldWidth; index++ {
		screen.SetContent(x+index, y, ' ', nil, fieldStyle)
	}

	// Text.
	var cursorScreenPos int

	text := i.text
	if fieldWidth >= stringWidth(text) {
		// We have enough space for the full text.
		fw := fieldWidth

		for j, c := range text {
			cStr := string(c)
			color := i.fieldTextColor
			if val, ok := i.textColors[j]; ok {
				color = val
			}
			_, drawnWidth := tview.Print(screen, tview.Escape(cStr), x+(fieldWidth-fw), y, fw, tview.AlignLeft, color)
			fw -= drawnWidth
		}
		i.offset = 0
		iterateString(text, func(main rune, comb []rune, textPos, textWidth, screenPos, screenWidth int) bool {
			if textPos >= i.cursorPos {
				return true
			}
			cursorScreenPos += screenWidth
			return false
		})
	} else {
		// The text doesn't fit. Where is the cursor?
		if i.cursorPos < 0 {
			i.cursorPos = 0
		} else if i.cursorPos > len(text) {
			i.cursorPos = len(text)
		}
		// Shift the text so the cursor is inside the field.
		var shiftLeft int
		if i.offset > i.cursorPos {
			i.offset = i.cursorPos
		} else if subWidth := stringWidth(text[i.offset:i.cursorPos]); subWidth > fieldWidth-1 {
			shiftLeft = subWidth - fieldWidth + 1
		}
		currentOffset := i.offset
		iterateString(text, func(main rune, comb []rune, textPos, textWidth, screenPos, screenWidth int) bool {
			if textPos >= currentOffset {
				if shiftLeft > 0 {
					i.offset = textPos + textWidth
					shiftLeft -= screenWidth
				} else {
					if textPos+textWidth > i.cursorPos {
						return true
					}
					cursorScreenPos += screenWidth
				}
			}
			return false
		})
		fw := fieldWidth
		for j := i.offset; j < len(text); j++ {
			cStr := string(text[j])
			color := i.fieldTextColor
			if val, ok := i.textColors[j]; ok {
				color = val
			}
			_, drawnWidth := tview.Print(screen, tview.Escape(cStr), x+(fieldWidth-fw), y, fw, tview.AlignLeft, color)
			fw -= drawnWidth
		}
	}

	// Set cursor.
	if i.hasFocus {
		screen.ShowCursor(x+cursorScreenPos, y)
	}
}

// InputHandler returns the handler for this primitive.
func (i *InputField) InputHandler() func(event *tcell.EventKey, setFocus func(p tview.Primitive)) {
	return i.WrapInputHandler(func(event *tcell.EventKey, setFocus func(p tview.Primitive)) {
		// Trigger changed events.
		currentText := i.text
		defer func() {
			if i.text != currentText {
				if i.changed != nil {
					i.changed(i.text)
				}
			}
		}()

		// Movement functions.
		home := func() { i.cursorPos = 0 }
		end := func() { i.cursorPos = len(i.text) }
		moveLeft := func() {
			iterateStringReverse(i.text[:i.cursorPos], func(main rune, comb []rune, textPos, textWidth, screenPos, screenWidth int) bool {
				i.cursorPos -= textWidth
				return true
			})
		}
		moveRight := func() {
			iterateString(i.text[i.cursorPos:], func(main rune, comb []rune, textPos, textWidth, screenPos, screenWidth int) bool {
				i.cursorPos += textWidth
				return true
			})
		}
		moveWordLeft := func() {
			i.cursorPos = len(regexp.MustCompile(`\S+\s*$`).ReplaceAllString(i.text[:i.cursorPos], ""))
		}
		moveWordRight := func() {
			i.cursorPos = len(i.text) - len(regexp.MustCompile(`^\s*\S+\s*`).ReplaceAllString(i.text[i.cursorPos:], ""))
		}

		// Add character function. Returns whether or not the rune character is
		// accepted.
		add := func(r rune) bool {
			newText := i.text[:i.cursorPos] + string(r) + i.text[i.cursorPos:]
			i.text = newText
			i.cursorPos += len(string(r))
			return true
		}

		// Finish up.
		finish := func(key tcell.Key) {}

		// Process key event.
		switch key := event.Key(); key {
		case tcell.KeyRune: // Regular character.
			switch {
			case event.Modifiers()&tcell.ModAlt > 0:
				// We accept some Alt- key combinations.
				switch event.Rune() {
				case 'a': // Home.
					home()
				case 'e': // End.
					end()
				case 'b': // Move word left.
					moveWordLeft()
				case 'f': // Move word right.
					moveWordRight()
				default:
					if !add(event.Rune()) {
						return
					}
				}
			default:
				// Other keys are simply accepted as regular characters.
				if !add(event.Rune()) {
					return
				}
			}
		case tcell.KeyCtrlU: // Delete all.
			i.text = ""
			i.cursorPos = 0
		case tcell.KeyCtrlK: // Delete until the end of the line.
			i.text = i.text[:i.cursorPos]
		case tcell.KeyCtrlW: // Delete last word.
			lastWord := regexp.MustCompile(`\S+\s*$`)
			newText := lastWord.ReplaceAllString(i.text[:i.cursorPos], "") + i.text[i.cursorPos:]
			i.cursorPos -= len(i.text) - len(newText)
			i.text = newText
		case tcell.KeyBackspace, tcell.KeyBackspace2: // Delete character before the cursor.
			iterateStringReverse(i.text[:i.cursorPos], func(main rune, comb []rune, textPos, textWidth, screenPos, screenWidth int) bool {
				i.text = i.text[:textPos] + i.text[textPos+textWidth:]
				i.cursorPos -= textWidth
				return true
			})
			if i.offset >= i.cursorPos {
				i.offset = 0
			}
		case tcell.KeyDelete: // Delete character after the cursor.
			iterateString(i.text[i.cursorPos:], func(main rune, comb []rune, textPos, textWidth, screenPos, screenWidth int) bool {
				i.text = i.text[:i.cursorPos] + i.text[i.cursorPos+textWidth:]
				return true
			})
		case tcell.KeyLeft:
			if event.Modifiers()&tcell.ModAlt > 0 {
				moveWordLeft()
			} else {
				moveLeft()
			}
		case tcell.KeyRight:
			if event.Modifiers()&tcell.ModAlt > 0 {
				moveWordRight()
			} else {
				moveRight()
			}
		case tcell.KeyHome, tcell.KeyCtrlA:
			home()
		case tcell.KeyEnd, tcell.KeyCtrlE:
			end()
		case tcell.KeyEnter, tcell.KeyEscape: // We might be done.
			finish(key)
		case tcell.KeyDown, tcell.KeyTab: // Autocomplete selection.
			finish(key)
		case tcell.KeyUp, tcell.KeyBacktab: // Autocomplete selection.
			finish(key)
		}
	})
}

// MouseHandler returns the mouse handler for this primitive.
func (i *InputField) MouseHandler() func(action tview.MouseAction, event *tcell.EventMouse, setFocus func(p tview.Primitive)) (bool, tview.Primitive) {
	return i.WrapMouseHandler(func(action tview.MouseAction, event *tcell.EventMouse, setFocus func(p tview.Primitive)) (bool, tview.Primitive) {
		x, y := event.Position()
		_, rectY, _, _ := i.GetInnerRect()
		if !i.InRect(x, y) {
			return false, nil
		}

		consumed := false
		// Process mouse event.
		if action == tview.MouseLeftClick && y == rectY {
			// Determine where to place the cursor.
			if x >= i.fieldX {
				if !iterateString(i.text[i.offset:], func(main rune, comb []rune, textPos int, textWidth int, screenPos int, screenWidth int) bool {
					if x-i.fieldX < screenPos+screenWidth {
						i.cursorPos = textPos + i.offset
						return true
					}
					return false
				}) {
					i.cursorPos = len(i.text)
				}
			}
			setFocus(i)
			consumed = true
		}

		return consumed, nil
	})
}

// Focus is called when this primitive receives focus.
func (i *InputField) Focus(func(p tview.Primitive)) {
	i.hasFocus = true
}

// The following util functions are copied from
// https://github.com/rivo/tview/blob/e352ec01560d2efc19918e4732310255b2bb800a/util.go and are unmodified.

// stringWidth returns the number of horizontal cells needed to print the given
// text. It splits the text into its grapheme clusters, calculates each
// cluster's width, and adds them up to a total.
func stringWidth(text string) int {
	width := 0
	g := uniseg.NewGraphemes(text)
	for g.Next() {
		var chWidth int
		for _, r := range g.Runes() {
			chWidth = runewidth.RuneWidth(r)
			if chWidth > 0 {
				break // Our best guess at this point is to use the width of the first non-zero-width rune.
			}
		}
		width += chWidth
	}
	return width
}

// iterateString iterates through the given string one printed character at a
// time. For each such character, the callback function is called with the
// Unicode code points of the character (the first rune and any combining runes
// which may be nil if there aren't any), the starting position (in bytes)
// within the original string, its length in bytes, the screen position of the
// character, and the screen width of it. The iteration stops if the callback
// returns true. This function returns true if the iteration was stopped before
// the last character.
func iterateString(text string, callback func(main rune, comb []rune, textPos, textWidth, screenPos, screenWidth int) bool) bool {
	var screenPos int

	gr := uniseg.NewGraphemes(text)
	for gr.Next() {
		r := gr.Runes()
		from, to := gr.Positions()
		width := stringWidth(gr.Str())
		var comb []rune
		if len(r) > 1 {
			comb = r[1:]
		}

		if callback(r[0], comb, from, to-from, screenPos, width) {
			return true
		}

		screenPos += width
	}

	return false
}

// iterateStringReverse iterates through the given string in reverse, starting
// from the end of the string, one printed character at a time. For each such
// character, the callback function is called with the Unicode code points of
// the character (the first rune and any combining runes which may be nil if
// there aren't any), the starting position (in bytes) within the original
// string, its length in bytes, the screen position of the character, and the
// screen width of it. The iteration stops if the callback returns true. This
// function returns true if the iteration was stopped before the last character.
func iterateStringReverse(text string, callback func(main rune, comb []rune, textPos, textWidth, screenPos, screenWidth int) bool) bool {
	type cluster struct {
		main                                       rune
		comb                                       []rune
		textPos, textWidth, screenPos, screenWidth int
	}

	// Create the grapheme clusters.
	var clusters []cluster
	iterateString(text, func(main rune, comb []rune, textPos int, textWidth int, screenPos int, screenWidth int) bool {
		clusters = append(clusters, cluster{
			main:        main,
			comb:        comb,
			textPos:     textPos,
			textWidth:   textWidth,
			screenPos:   screenPos,
			screenWidth: screenWidth,
		})
		return false
	})

	// Iterate in reverse.
	for index := len(clusters) - 1; index >= 0; index-- {
		if callback(
			clusters[index].main,
			clusters[index].comb,
			clusters[index].textPos,
			clusters[index].textWidth,
			clusters[index].screenPos,
			clusters[index].screenWidth,
		) {
			return true
		}
	}

	return false
}
