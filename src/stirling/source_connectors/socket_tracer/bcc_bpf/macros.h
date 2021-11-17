/*
 * This code runs using bpf in the Linux kernel.
 * Copyright 2018- The Pixie Authors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * SPDX-License-Identifier: GPL-2.0
 */

#pragma once

// Utility macro for use in BPF code, so the probe can exit if the symbol doesn't exist.
#define REQUIRE_SYMADDR(symaddr, retval) \
  if (symaddr == -1) {                   \
    return retval;                       \
  }

#define REQUIRE_LOCATION(loc, retval)     \
  if (loc.type == kLocationTypeInvalid) { \
    return retval;                        \
  }
