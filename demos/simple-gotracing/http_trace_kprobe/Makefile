
all: http_trace_kprobe

http_trace_kprobe: *.go
	go build -o $@ $^

clean:
	rm -f http_trace_kprobe
