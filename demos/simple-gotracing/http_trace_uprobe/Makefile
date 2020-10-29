
all: http_trace_uprobe

http_trace_uprobe: *.go
	go build -o $@ $^

clean:
	rm -f http_trace_uprobe
