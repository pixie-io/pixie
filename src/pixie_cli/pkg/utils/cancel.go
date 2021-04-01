package utils

import (
	"context"
	"os"
	"os/signal"
)

// WithSignalCancellable returns a context that will automatically be cancelled
// when Ctrl+C is pressed.
func WithSignalCancellable(ctx context.Context) context.Context {
	// trap Ctrl+C and call cancel on the context
	newCtx, cancel := context.WithCancel(ctx)
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt)
	defer func() {
		signal.Stop(c)
		cancel()
	}()

	go func() {
		select {
		case <-c:
			cancel()
		case <-newCtx.Done():
		}
	}()

	return newCtx
}
