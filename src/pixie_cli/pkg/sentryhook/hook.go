package sentryhook

import (
	"reflect"
	"time"

	"github.com/getsentry/sentry-go"
	"github.com/sirupsen/logrus"
)

var (
	levelMap = map[logrus.Level]sentry.Level{
		logrus.TraceLevel: sentry.LevelDebug,
		logrus.DebugLevel: sentry.LevelDebug,
		logrus.InfoLevel:  sentry.LevelInfo,
		logrus.WarnLevel:  sentry.LevelWarning,
		logrus.ErrorLevel: sentry.LevelError,
		logrus.FatalLevel: sentry.LevelFatal,
		logrus.PanicLevel: sentry.LevelFatal,
	}
)

// Converter converts log entry to sentry.
type Converter func(entry *logrus.Entry, event *sentry.Event, hub *sentry.Hub)

// Option are sentry options.
type Option func(h *Hook)

// Hook is a logrus hook structure.
type Hook struct {
	hub       *sentry.Hub
	levels    []logrus.Level
	tags      map[string]string
	extra     map[string]interface{}
	converter Converter
}

// New creates a new logrus hook.
func New(levels []logrus.Level, options ...Option) Hook {
	h := Hook{
		levels:    levels,
		hub:       sentry.CurrentHub(),
		converter: DefaultConverter,
	}

	for _, option := range options {
		option(&h)
	}

	return h
}

// WithTags adds tags.
func WithTags(tags map[string]string) Option {
	return func(h *Hook) {
		h.tags = tags
	}
}

// Levels returns the valid log levels to hook.
func (hook Hook) Levels() []logrus.Level {
	return hook.levels
}

// Fire sends an event to sentry.
func (hook Hook) Fire(entry *logrus.Entry) error {
	event := sentry.NewEvent()
	for k, v := range hook.extra {
		event.Extra[k] = v
	}
	for k, v := range hook.tags {
		event.Tags[k] = v
	}

	hook.converter(entry, event, hook.hub)

	hook.hub.CaptureEvent(event)
	hook.hub.Flush(2 * time.Second)
	return nil
}

// DefaultConverter is the default log converter.
func DefaultConverter(entry *logrus.Entry, event *sentry.Event, hub *sentry.Hub) {
	event.Level = levelMap[entry.Level]
	event.Message = entry.Message

	for k, v := range entry.Data {
		event.Extra[k] = v
	}

	if err, ok := entry.Data[logrus.ErrorKey].(error); ok {
		exception := sentry.Exception{
			Type:  reflect.TypeOf(err).String(),
			Value: err.Error(),
		}

		if hub.Client().Options().AttachStacktrace {
			exception.Stacktrace = sentry.ExtractStacktrace(err)
		}

		event.Exception = []sentry.Exception{exception}
	}
}
