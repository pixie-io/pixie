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

package cmd

import (
	"context"
	"os"
	"os/signal"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
)

func init() {
	levelStr := os.Getenv("LOG_LEVEL")
	level, err := log.ParseLevel(levelStr)
	if err != nil {
		level = log.InfoLevel
	}
	log.SetLevel(level)

	viper.AutomaticEnv()
	viper.SetEnvPrefix("PX")
}

func runCmdWithInterrruptableContext(f func(context.Context, *cobra.Command) error, cmd *cobra.Command) error {
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt)
	defer signal.Stop(c)
	go func() {
		<-c
		log.Info("Ignoring interrupt. Interrupt again to stop")
		<-c
		cancel()
		log.Info("Received interrupt. Cleaning up...")
		<-c
		log.Info("Received second interrupt. Exiting ungracefully...")
		os.Exit(1)
	}()
	return f(ctx, cmd)
}

// RootCmd is the base command used to add other commands.
var RootCmd = &cobra.Command{
	Use:   "root",
	Short: "root command used by other commands",
}
