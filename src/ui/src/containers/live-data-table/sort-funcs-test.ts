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

import { fieldSortFunc, serviceSortFunc } from './sort-funcs';

describe('fieldSortFunc', () => {
  it('correctly sorts the object by the specified subfield (with a value and record null)', () => {
    const f = fieldSortFunc('p99');
    const rows = [
      { p50: -20, p90: -26, p99: -23 },
      { p50: 10, p90: 60 },
      { p50: -30, p90: -36, p99: -33 },
      { p50: 20, p90: 260, p99: 261 },
      null,
      { p50: 30, p90: 360, p99: 370 },
    ];
    expect(rows.sort(f)).toStrictEqual([
      { p50: -30, p90: -36, p99: -33 },
      { p50: -20, p90: -26, p99: -23 },
      { p50: 20, p90: 260, p99: 261 },
      { p50: 30, p90: 360, p99: 370 },
      { p50: 10, p90: 60 },
      null,
    ]);
  });

  it('correctly sorts boolean values', () => {
    const f = fieldSortFunc('label');
    const rows = [
      { label: false },
      { label: true },
      { label: false },
      { label: true },
      { label: false },
      { label: false },
    ];
    expect(rows.sort(f)).toStrictEqual([
      { label: false },
      { label: false },
      { label: false },
      { label: false },
      { label: true },
      { label: true },
    ]);
  });

  it('correctly sorts bigint values', () => {
    const f = fieldSortFunc('label');
    const rows = [
      { label: BigInt(3) },
      { label: BigInt(1) },
      { label: BigInt(0) },
      { label: BigInt(2) },
      { label: BigInt(4) },
      { label: BigInt(5) },
    ];
    expect(rows.sort(f)).toStrictEqual([
      { label: BigInt(0) },
      { label: BigInt(1) },
      { label: BigInt(2) },
      { label: BigInt(3) },
      { label: BigInt(4) },
      { label: BigInt(5) },
    ]);
  });

  it('correctly sorts string values', () => {
    const f = fieldSortFunc('label');
    // The late Jurassic period, 164 to 145 million years ago, had very few (known) species considered to be dinosaurs.
    const rows = [
      /*
       * They _do_ move in herds! Nowadays, we don't have sauropods like this. At least giraffes are friendly!
       * Specimens have been found in Algeria, Portugal, Tanzania, and the United States.
       */
      { label: 'Brachiosaurus' },
      /*
       * The crocodile-sized gargoyle lizard lacks the morningstar at the tip of its Anklyosaurus cousin's tail.
       * Specimens have been found in the United States.
       */
      { label: 'Gargoyleosaurus' },
      /*
       * Visual evidence that chickens evolved from raptors. Feathered omnivores, just 1.7m long.
       * Specimens have been found in Canada.
       */
      { label: 'Chirostenotes' },
      /*
       * Tyrannosaurus Rex was not alive during the late Jurassic era. Its smaller cousin, Allosaurus, was.
       * Specimens have been found in Portugal and the United States.
       */
      { label: 'Allosaurus' },
      /*
       * A dwarf sauropod measuring about 6.2 meters snout to tail, with a recognizable bump atop its head.
       * Specimens have been found in Germany.
       */
      { label: 'Europasaurus' },
      /*
       * Also known as the "oak lizard", Dryosaurus is a bipedal dinosaur about four meters snout to tail.
       * Specimens have been found in Tanzania and the United States.
       */
      { label: 'Dryosaurus' },
    ];
    expect(rows.sort(f)).toStrictEqual([
      // Surprisingly, there are no dinosaurs from the late Jurassic period whose names start with an F.
      { label: 'Allosaurus' },
      { label: 'Brachiosaurus' },
      { label: 'Chirostenotes' },
      { label: 'Dryosaurus' },
      { label: 'Europasaurus' },
      { label: 'Gargoyleosaurus' },
    ]);
  });

  it('throws when trying to compare disparate types', () => {
    const f = fieldSortFunc('label');
    const rows = [
      { label: 'I am not a number.' },
      { label: 12345 },
    ];
    expect(() => rows.sort(f)).toThrowError();
  });

  it('throws when trying to compare types with no reasonable comparison method', () => {
    const f = fieldSortFunc('label');
    expect(() => [
      { label: function noSortingMe() {} },
      { label: function notMeEither() {} },
    ].sort(f)).toThrowError();

    expect(() => [
      { label: {} },
      { label: {} },
    ].sort(f)).toThrowError();

    expect(() => [
      { label: Symbol('Symbols cannot be compared reasonably') },
      { label: Symbol('Because doing so would be quite dirty') },
    ].sort(f)).toThrowError();
  });

  it('sorts nulls to the end of the list', () => {
    const f = fieldSortFunc('label');
    expect([
      { label: 'foo' },
      { label: null },
      { label: 'bar' },
    ].sort(f)).toStrictEqual([
      { label: 'bar' },
      { label: 'foo' },
      { label: null },
    ]);
  });
});

describe('serviceSortFunc', () => {
  it('Sorts service names both in plain strings and in arrays', () => {
    const base = [
      'foo',
      ['bar', 'foo'],
      ['a', 'foo'],
      'bar',
      'a',
      ['a-b.c;d?e', 'bar'],
      'a-b.c;d?e',
      ['foo', 'bar'],
    ].map((s) => (Array.isArray(s) ? JSON.stringify(s) : s));

    const out = [...base].sort(serviceSortFunc());
    expect(out).toEqual([
      'a',
      '["a","foo"]',
      'a-b.c;d?e',
      '["a-b.c;d?e","bar"]',
      'bar',
      '["bar","foo"]',
      'foo',
      '["foo","bar"]',
    ]);
  });
});
