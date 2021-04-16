package formatters

import (
	"context"
	"fmt"
	"io"

	"github.com/olekukonko/tablewriter"

	"px.dev/pixie/src/api/go/pxapi/errdefs"
	"px.dev/pixie/src/api/go/pxapi/types"
)

// TableFormatter formats data as table.
type TableFormatter struct {
	w            io.Writer
	tableName    string
	headerValues []string
	data         [][]string
}

// TableFormatterOption configures options on the formatter.
type TableFormatterOption func(t *TableFormatter)

// NewTableFormatter creates a TableFormatter with the specified options.
func NewTableFormatter(w io.Writer, opts ...TableFormatterOption) (*TableFormatter, error) {
	t := &TableFormatter{
		w: w,
	}
	for _, opt := range opts {
		opt(t)
	}
	return t, nil
}

// HandleInit is called when the table metadata is available.
func (t *TableFormatter) HandleInit(ctx context.Context, metadata types.TableMetadata) error {
	if len(t.tableName) != 0 {
		return fmt.Errorf("%w: did not expect init to be called more than once", errdefs.ErrInternalDuplicateTableMetadata)
	}
	t.tableName = metadata.Name
	for _, col := range metadata.ColInfo {
		t.headerValues = append(t.headerValues, col.Name)
	}
	return nil
}

// HandleRecord is called for each record of the table.
func (t *TableFormatter) HandleRecord(ctx context.Context, record *types.Record) error {
	if len(record.Data) != len(t.headerValues) {
		return fmt.Errorf("%w: mismatch in header and data sizes", errdefs.ErrInvalidArgument)
	}
	var r []string
	for _, d := range record.Data {
		r = append(r, d.String())
	}
	t.data = append(t.data, r)
	return nil
}

// HandleDone is called when all data has been streamed.
func (t *TableFormatter) HandleDone(ctx context.Context) error {
	fmt.Fprintf(t.w, "Table: %s\n", t.tableName)
	table := tablewriter.NewWriter(t.w)
	table.SetHeader(t.headerValues)

	table.SetAutoFormatHeaders(true)
	table.SetAutoWrapText(false)
	table.SetHeaderAlignment(tablewriter.ALIGN_LEFT)
	table.SetAlignment(tablewriter.ALIGN_LEFT)
	table.SetColWidth(30)
	table.SetReflowDuringAutoWrap(true)
	table.SetCenterSeparator("")
	table.SetColumnSeparator("")
	table.SetRowSeparator("")
	table.SetHeaderLine(false)
	table.SetBorder(false)
	table.SetTablePadding("\t")
	table.SetNoWhiteSpace(false)

	for _, row := range t.data {
		table.Append(row)
	}

	table.Render()
	return nil
}
