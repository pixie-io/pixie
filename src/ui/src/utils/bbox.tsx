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

// Determines if elem is fully visible inside parent. If not, this can be used with
// .scrollIntoView() to make elem visible.
// NOTE: this only checks the vertical position of the elements.
export function isInView(parent: HTMLElement, elem: HTMLElement): boolean {
  const pbbox = parent.getBoundingClientRect();
  const ebbox = elem.getBoundingClientRect();
  return pbbox.top <= ebbox.top && pbbox.bottom >= ebbox.bottom;
}
