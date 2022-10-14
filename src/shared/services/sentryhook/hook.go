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

package sentryhook

import (
	"reflect"
	"time"

	"github.com/getsentry/sentry-go"
	log "github.com/sirupsen/logrus"
)

var (
	levelMap = map[log.Level]sentry.Level{
		log.TraceLevel: sentry.LevelDebug,
		log.DebugLevel: sentry.LevelDebug,
		log.InfoLevel:  sentry.LevelInfo,
		log.WarnLevel:  sentry.LevelWarning,
		log.ErrorLevel: sentry.LevelError,
		log.FatalLevel: sentry.LevelFatal,
		log.PanicLevel: sentry.LevelFatal,
	}
)

// Converter converts log entry to sentry.
type Converter func(entry *log.Entry, event *sentry.Event, hub *sentry.Hub)

// Option are sentry options.
type Option func(h *Hook)

// Hook is a logrus hook structure.
type Hook struct {
	hub       *sentry.Hub
	levels    []log.Level
	tags      map[string]string
	extra     map[string]interface{}
	converter Converter
}

// New creates a new logrus hook.
func New(levels []log.Level, options ...Option) Hook {
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
func (hook Hook) Levels() []log.Level {
	return hook.levels
}

// Fire sends an event to sentry.
func (hook Hook) Fire(entry *log.Entry) error {
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
func DefaultConverter(entry *log.Entry, event *sentry.Event, hub *sentry.Hub) {
	event.Level = levelMap[entry.Level]
	event.Message = entry.Message

	for k, v := range entry.Data {
		event.Extra[k] = v
	}

	if err, ok := entry.Data[log.ErrorKey].(error); ok {
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
