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

import * as React from 'react';
import { ApolloError } from '@apollo/client';

/**
 * Shorthand names for long types, particularly those with nested generics or long names.
 */

export type SetStateFunc<T> = React.Dispatch<React.SetStateAction<T>>;

/**
 * The output of a Pixie API React hook, for queries that provide read-only data.
 * This behaves like MutablePixieQueryResult with the setter (second member) omitted.
 */
export type ImmutablePixieQueryResult<T> = [T|undefined, boolean, ApolloError?];
/** This is the same thing as ImmutablePixieQueryResult, but a value is always present even before data is loaded. */
export type ImmutablePixieQueryGuaranteedResult<T> = [T, boolean, ApolloError?];

/**
 * The output of a Pixie API React hook, for queries that provide data that can be overwritten.
 * This behaves similarly to React.useState, with some extra properties:
 * - No initial value is provided, and instead `undefined` is returned for the stored value until data has loaded.
 * - A boolean `loading` flag
 * - An error, if one occurred. Undefined if the query has not thrown an error.
 */
export type MutablePixieQueryResult<T> = [T|undefined, SetStateFunc<T>, boolean, ApolloError?];
/** This is the same thing as MutablePixieQueryResult, but a value is always present even before data is loaded. */
export type MutablePixieQueryGuaranteedResult<T> = [T, SetStateFunc<T>, boolean, ApolloError?];
