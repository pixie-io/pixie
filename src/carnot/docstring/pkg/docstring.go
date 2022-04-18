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

package docstring

import (
	"fmt"
	re "regexp"
	"sort"
	"strings"

	"px.dev/pixie/src/carnot/docspb"
	"px.dev/pixie/src/carnot/udfspb"
	"px.dev/pixie/src/utils"
)

// getMatch returns the regex match of Subexpname `name` in `s`.
func getMatch(s string, name string, rx string) string {
	allRe := re.MustCompile(rx)
	stringsubMatch := allRe.FindStringSubmatch(s)
	for i, a := range stringsubMatch {
		if allRe.SubexpNames()[i] == name {
			return a
		}
	}
	return ""
}

func (w *parser) GetArgType(s string) string {
	return getMatch(s, "type", w.ArgReStr())
}

func (w *parser) GetArgIdent(s string) string {
	return getMatch(s, "ident", w.ArgReStr())
}

func (w *parser) GetArgDesc(s string) string {
	return getMatch(s, "desc", w.ArgReStr())
}

func (w *parser) GetRetType(s string) string {
	if !w.ReturnMatch(s) {
		panic(fmt.Errorf("GetRetType doesn't match %s", s))
	}
	return getMatch(s, "type", w.ReturnReStr())
}

func (w *parser) GetRetDesc(s string) string {
	if !w.ReturnMatch(s) {
		panic(fmt.Errorf("GetRetDesc doesn't match %s", s))
	}
	return getMatch(s, "desc", w.ReturnReStr())
}

func (w *parser) ArgReStr() string {
	return fmt.Sprintf(`(?P<ident>%s) \((?P<type>%s)\): (?P<desc>%s)`, w.identRegex, w.typeRegex, w.descRegex)
}

func (w *parser) ReturnReStr() string {
	return fmt.Sprintf(`(?P<type>%s): (?P<desc>%s)`, w.typeRegex, w.descRegex)
}

func (w *parser) ArgMatch(s string) bool {
	allRe := re.MustCompile(w.ArgReStr())

	return allRe.MatchString(s) && w.isTabbedLine(s)
}
func (w *parser) ReturnMatch(s string) bool {
	allRe := re.MustCompile(w.ReturnReStr())

	return allRe.MatchString(s) && w.isTabbedLine(s)
}

func isContinuationLine(s string, tabChar string) bool {
	mc := re.MustCompile(fmt.Sprintf(`%s([^\s]*)`, tabChar))
	return len(strings.TrimSpace(s)) == 0 || mc.MatchString(s)
}

type parser struct {
	// regex string to match identifiers.
	identRegex string
	// regex string to match types.
	typeRegex string
	// regex string to match argument/return descriptions.
	descRegex string
	// String that represents tabs. Could be a tab or multiple strings.
	tabChar string
	// The regex matchers for section headers.
	argHeadMatch      *re.Regexp
	examplesHeadMatch *re.Regexp
	returnHeadMatch   *re.Regexp
	// The structured object to write the documentation.
	parsedDoc *FunctionDocstring
}

func (w *parser) MatchArgHeader(line string) bool {
	return w.argHeadMatch.MatchString(line)
}

func (w *parser) MatchReturnHeader(line string) bool {
	return w.returnHeadMatch.MatchString(line)
}

func (w *parser) MatchExamplesHeader(line string) bool {
	return w.examplesHeadMatch.MatchString(line)
}

func (w *parser) isTabbedLine(line string) bool {
	// Don't match lines that only contain spaces and if tabChar is an empty string.
	if len(strings.TrimSpace(line)) == 0 || len(w.tabChar) == 0 {
		return false
	}

	r := re.MustCompile(fmt.Sprintf(`^%s`, w.tabChar))
	return r.MatchString(line)
}

func (w *parser) Walk(lines *[]string, i int) (int, error) {
	if i >= len(*lines) {
		return i, nil
	}

	if i == 0 {
		w.parsedDoc.body.Brief = (*lines)[i]
		return w.Walk(lines, i+1)
	}

	if w.isTabbedLine((*lines)[i]) {
		rx := re.MustCompile(`^[ \t]*`)
		tabs := rx.FindString((*lines)[i])
		return i, formatLineError(lines, i, fmt.Sprintf("Unexpected indent '%s'", tabs))
	}

	if w.MatchExamplesHeader((*lines)[i]) {
		return w.processExamples(lines, i+1)
	}

	if w.MatchArgHeader((*lines)[i]) {
		return w.processArguments(lines, i+1)
	}

	if w.MatchReturnHeader((*lines)[i]) {
		return w.processReturn(lines, i+1)
	}

	if len(w.parsedDoc.body.Desc) == 0 {
		w.parsedDoc.body.Desc = (*lines)[i]
	} else {
		line := (*lines)[i]
		// Add newline if starts with *.
		if ok, err := re.MatchString(`^\*\s`, line); err == nil && ok {
			line = "\n" + line
		}
		w.parsedDoc.body.Desc = w.parsedDoc.body.Desc + " " + line
	}

	return w.Walk(lines, i+1)
}

func (w *parser) processExamples(lines *[]string, i int) (int, error) {
	example := make([]string, 0)
	for i < len(*lines) {
		if !re.MustCompile(fmt.Sprintf("^%[1]s[^%[1]s].*", w.tabChar)).MatchString((*lines)[i]) {
			break
		}
		example = append(example, strings.Trim((*lines)[i], w.tabChar))
		i++
	}
	w.parsedDoc.body.Examples = append(w.parsedDoc.body.Examples, &docspb.ExampleDoc{
		Value: fmt.Sprintf("```\n%s\n```", strings.Join(example, "\n")),
	})
	return w.Walk(lines, i)
}

func (w *parser) processArguments(lines *[]string, i int) (int, error) {
	if i >= len(*lines) || !w.ArgMatch((*lines)[i]) {
		return i, formatLineError(lines, i, "Expected arg description line")
	}
	argDoc := &docspb.IdentDoc{}
	argDoc.Ident = w.GetArgIdent((*lines)[i])
	argDoc.Types = []string{w.GetArgType((*lines)[i])}
	argDoc.Desc = w.GetArgDesc((*lines)[i])
	i++

	i = processRemainingDesc(lines, i, w.tabChar+w.tabChar, argDoc)

	w.parsedDoc.function.Args = append(w.parsedDoc.function.Args, argDoc)

	// Continue to process args if they match.
	if i < len(*lines) && w.ArgMatch((*lines)[i]) {
		return w.processArguments(lines, i)
	}

	return w.Walk(lines, i)
}

func (w *parser) processReturn(lines *[]string, i int) (int, error) {
	if i >= len(*lines) {
		return i, formatLineError(lines, i, "Expected return description line")
	}
	if !w.ReturnMatch((*lines)[i]) {
		return i, formatLineError(lines, i, "Expected return description line")
	}
	argDoc := &docspb.IdentDoc{}
	argDoc.Types = []string{w.GetRetType((*lines)[i])}
	argDoc.Desc = w.GetRetDesc((*lines)[i])
	i++
	i = processRemainingDesc(lines, i, w.tabChar, argDoc)
	// Match the next lines
	w.parsedDoc.function.ReturnType = argDoc
	return w.Walk(lines, i)
}

// hasContent returns true if there is non-space characters on this line.
func hasContent(s string) bool {
	return len(strings.TrimSpace(s)) > 0
}

func processRemainingDesc(lines *[]string, i int, tabChar string, argDoc *docspb.IdentDoc) int {
	for i < len(*lines) {
		if !isContinuationLine((*lines)[i], tabChar) {
			break
		}
		if hasContent((*lines)[i]) {
			argDoc.Desc += " " + strings.Trim((*lines)[i], tabChar)
		} else {
			argDoc.Desc += "\n"
		}
		i++
	}
	argDoc.Desc = strings.TrimSpace(argDoc.Desc)
	return i
}

func combineLines(lines *[]string, lineNs []int, highlightLine int) string {
	out := make([]string, len(lineNs))
	for i, l := range lineNs {
		if l >= len(*lines) || l < 0 {
			panic(fmt.Sprintf("invalid line %d. Allowed (0, %d)", l, len(*lines)))
		}
		firstChar := "  "
		if highlightLine == l {
			// Insert a carrot if this is the higlight line.
			firstChar = "> "
		}
		out[i] = fmt.Sprintf("%s%3d %s", firstChar, l, (*lines)[l])
	}
	return strings.Join(out, "\n")
}

func combineNeighborLines(lines *[]string, i int) string {
	if i+1 >= len(*lines) {
		l := len(*lines)
		return combineLines(lines, []int{l - 3, l - 2, l - 1}, i)
	}
	if i-1 <= 0 {
		return combineLines(lines, []int{0, 1, 2}, i)
	}

	return combineLines(lines, []int{i - 1, i, i + 1}, i)
}

// formatLineError takes the lines object and prints the error with a pointer to the line that failed.
// Simplifies debugging.
func formatLineError(lines *[]string, i int, msg string) error {
	return fmt.Errorf("%s \n%s", msg, indent(combineNeighborLines(lines, i)))
}

// FunctionDocstring is the generic structure that contains documentation for functions.
type FunctionDocstring struct {
	body     *docspb.DocBody
	function *docspb.FuncDoc
}

func isMultiTab(multiTab string, tab string) bool {
	// Makes sure that the multiTab is actually a multiple of the tab.
	return re.MustCompile(fmt.Sprintf("^(%s)+$", tab)).MatchString(multiTab)
}

// Parses the docstring and returns the tabChar. Errors out if there are disagreeing tabCharacters.
func getTabChar(docstring string) (string, error) {
	var tabChar string
	ea := utils.MakeErrorAccumulator()
	lines := strings.Split(docstring, "\n")
	for i, l := range lines {
		indentStr := getMatch(l, "indent", `^(?P<indent>(\t|\s*)).*`)
		if len(indentStr) == 0 {
			continue
		}
		if len(tabChar) > 0 && tabChar != indentStr {
			// The indent string could be a multiple of the tab.
			if !isMultiTab(indentStr, tabChar) {
				ea.AddError(formatLineError(&lines, i, fmt.Sprintf("mistmached tab '%s'", indentStr)))
			}
		} else {
			tabChar = indentStr
		}
	}
	mergedErr := ea.Merge()
	if mergedErr != nil {
		return "", fmt.Errorf("Mismatching tabChars '%s': \n%s", tabChar, mergedErr.Error())
	}
	return tabChar, nil
}

func dedent(s string) string {
	var firstLine string
	for _, l := range strings.Split(s, "\n") {
		if len(l) > 0 {
			firstLine = l
			break
		}
	}
	indentStr := getMatch(firstLine, "indent", `^(?P<indent>\s*).*`)
	if len(indentStr) == 0 {
		return s
	}
	outLines := make([]string, 0)
	rrr := re.MustCompile(fmt.Sprintf(`^%s`, indentStr))
	for _, l := range strings.Split(s, "\n") {
		outLines = append(outLines, rrr.ReplaceAllString(l, ""))
	}
	return strings.Join(outLines, "\n")
}

// parseDocstring parses the docstring into a body and function.
func parseDocstring(docString string) (*FunctionDocstring, error) {
	tabChar, err := getTabChar(docString)
	if err != nil {
		return nil, err
	}

	firstChar := `[_a-zA-z]`
	p := parser{
		identRegex:        fmt.Sprintf(`\*?\*?%s\w*`, firstChar),
		typeRegex:         fmt.Sprintf(`%s[^:]*?`, firstChar),
		descRegex:         ".*",
		tabChar:           tabChar,
		argHeadMatch:      re.MustCompile(`^Args:[\s]*$`),
		examplesHeadMatch: re.MustCompile("^Examples:"),
		returnHeadMatch:   re.MustCompile(`^Returns:[\s]*$`),
		parsedDoc: &FunctionDocstring{
			body: &docspb.DocBody{
				Examples: make([]*docspb.ExampleDoc, 0),
			},
			function: &docspb.FuncDoc{
				Args: make([]*docspb.IdentDoc, 0),
			},
		},
	}

	lines := strings.Split(docString, "\n")
	_, err = p.Walk(&lines, 0)
	if err != nil {
		return nil, err
	}

	return p.parsedDoc, nil
}

const topicRegex = `:topic: (?P<topic>[^\s]*)\n`
const opnameRegex = `:opname: (?P<opname>.*)\n`

// getTag finds the tag in the docstring if it exists.
func getTag(docstring, tagRegex string) string {
	r := re.MustCompile(tagRegex)

	m := r.FindStringSubmatch(docstring)
	if len(m) == 0 {
		return ""
	}
	return string(m[1])
}

// removeTag removes the tag from the docstring for cleaner parsing
func removeTag(docstring, tagRegex string) string {
	r := re.MustCompile(tagRegex)
	return r.ReplaceAllString(docstring, "")
}

// getTopic finds the topic in the docstring if it exists.
func getTopic(docstring string) string {
	return getTag(docstring, topicRegex)
}

// removeTopic removes the topic from the docstring for cleaner parsing
func removeTopic(docstring string) string {
	return removeTag(docstring, topicRegex)
}

// PixieMutation is the topic for any state changes.
const PixieMutation = "pixie_state_management"

// TracepointDecorator is the topic for the tracepoint decorator.
const TracepointDecorator = "tracepoint_decorator"

// TracepointFields is the topic for the tracepoint fields.
const TracepointFields = "tracepoint_fields"

// CompileTimeFns is the topic for functions that execute at compile time.
const CompileTimeFns = "compile_time_fn"

// DataFrameOps topic is for dataframe operations.
const DataFrameOps = "dataframe_ops"

// OTelFunctions is the topic for PxL OpenTelemetry exporter functions.
const OTelFunctions = "otel"

// Parses the docstring and writes the result to the structured docs.
func parseDocstringAndWrite(outDocs *docspb.StructuredDocs, rawDocstring string, name string) error {
	topic := getTopic(rawDocstring)
	if len(topic) == 0 {
		return nil
	}
	docstring := removeTopic(rawDocstring)

	docstring = removeTag(docstring, opnameRegex)

	docstring = strings.TrimSpace(dedent(docstring))

	// For now we just figure out which part of outDocs to write the docstring. In the future, we might
	// parse each topic differently.
	genDocString, err := parseDocstring(docstring)
	if err != nil {
		return err
	}
	genDocString.body.Name = name

	switch topic {
	case PixieMutation:
		outDocs.MutationDocs = append(outDocs.MutationDocs, &docspb.MutationDoc{
			Body:    genDocString.body,
			FuncDoc: genDocString.function,
		})
	case TracepointDecorator:
		outDocs.TracepointDecoratorDocs = append(outDocs.TracepointDecoratorDocs, &docspb.TracepointDecoratorDoc{
			Body:    genDocString.body,
			FuncDoc: genDocString.function,
		})
	case TracepointFields:
		outDocs.TracepointFieldDocs = append(outDocs.TracepointFieldDocs, &docspb.TracepointFieldDoc{
			Body:    genDocString.body,
			FuncDoc: genDocString.function,
		})
	case DataFrameOps:
		body := genDocString.body
		outDocs.DataframeOpDocs = append(outDocs.DataframeOpDocs, &docspb.DataFrameOpDoc{
			Body:    body,
			FuncDoc: genDocString.function,
		})
	case CompileTimeFns:
		outDocs.CompileFnDocs = append(outDocs.CompileFnDocs, &docspb.CompileFnDoc{
			Body:    genDocString.body,
			FuncDoc: genDocString.function,
		})
	case OTelFunctions:
		body := genDocString.body
		outDocs.OTelDocs = append(outDocs.OTelDocs, &docspb.OTelDoc{
			Body:    body,
			FuncDoc: genDocString.function,
		})

	default:
		return fmt.Errorf("topic not found %s", topic)
	}

	return nil
}

func makeName(runningName, addedName string) string {
	if len(runningName) > 0 {
		return fmt.Sprintf("%s.%s", runningName, addedName)
	}
	return addedName
}

func indent(i string) string {
	return "\t" + strings.Join(strings.Split(i, "\n"), "\n\t")
}

func isAllowed(n string) bool {
	return n != "px.DataFrame.DataFrame"
}

// parseDocstringTree traverses the DocstringNode tree and parses docstrings into the StructuredDocs.
func parseDocstringTree(node *docspb.DocstringNode, outDocs *docspb.StructuredDocs, currentName string) error {
	ea := utils.MakeErrorAccumulator()
	name := makeName(currentName, node.Name)
	if isAllowed(name) {
		err := parseDocstringAndWrite(outDocs, node.Docstring, name)
		if err != nil {
			// We don't early exit because we want to surface all the errors rather than having to restart.
			ea.AddError(fmt.Errorf("name: %s, err:%s", name, err.Error()))
		}
	}
	// Recurse on the Children.
	for _, d := range node.Children {
		err := parseDocstringTree(d, outDocs, name)
		if err != nil {
			// We don't early exit because we want to surface all the errors rather than having to restart.
			ea.AddError(err)
		}
	}
	mergedErr := ea.Merge()
	if mergedErr != nil {
		return fmt.Errorf("%s: \n%s", currentName, mergedErr.Error())
	}
	return nil
}

func processUDFDocs(udfs *udfspb.Docs) (*udfspb.Docs, error) {
	if udfs == nil {
		return udfs, nil
	}
	for _, u := range udfs.Udf {
		for _, e := range u.Examples {
			e.Value = fmt.Sprintf("```\n%s\n```", e.Value)
		}
	}
	return udfs, nil
}

// ParseAllDocStrings takes the doc object and returns a new version where its docstring and children Docs
// are parsed.
func ParseAllDocStrings(iDoc *docspb.InternalPXLDocs) (*docspb.StructuredDocs, error) {
	newUdfDocs, err := processUDFDocs(iDoc.UdfDocs)
	ea := utils.MakeErrorAccumulator()
	if err != nil {
		ea.AddError(err)
	}
	newDoc := &docspb.StructuredDocs{
		UdfDocs: newUdfDocs,
	}

	for _, d := range iDoc.DocstringNodes {
		err := parseDocstringTree(d, newDoc, "")
		ea.AddError(err)
	}

	// sort the slices.
	sort.SliceStable(newDoc.MutationDocs, func(i, j int) bool {
		return newDoc.MutationDocs[i].Body.Name < newDoc.MutationDocs[j].Body.Name
	})
	sort.SliceStable(newDoc.TracepointDecoratorDocs, func(i, j int) bool {
		return newDoc.TracepointDecoratorDocs[i].Body.Name < newDoc.TracepointDecoratorDocs[j].Body.Name
	})
	sort.SliceStable(newDoc.TracepointFieldDocs, func(i, j int) bool {
		return newDoc.TracepointFieldDocs[i].Body.Name < newDoc.TracepointFieldDocs[j].Body.Name
	})
	sort.SliceStable(newDoc.DataframeOpDocs, func(i, j int) bool {
		return newDoc.DataframeOpDocs[i].Body.Name < newDoc.DataframeOpDocs[j].Body.Name
	})
	sort.SliceStable(newDoc.CompileFnDocs, func(i, j int) bool {
		return newDoc.CompileFnDocs[i].Body.Name < newDoc.CompileFnDocs[j].Body.Name
	})
	sort.SliceStable(newDoc.OTelDocs, func(i, j int) bool {
		return newDoc.OTelDocs[i].Body.Name < newDoc.OTelDocs[j].Body.Name
	})
	sort.SliceStable(newDoc.UdfDocs.Udf, func(i, j int) bool {
		return newDoc.UdfDocs.Udf[i].Name < newDoc.UdfDocs.Udf[j].Name
	})

	errMerged := ea.Merge()
	if errMerged != nil {
		return nil, errMerged
	}
	return newDoc, nil
}
