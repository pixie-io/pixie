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

#ifdef PL_IR_NODE

PL_IR_NODE(MemorySource)
PL_IR_NODE(MemorySink)
PL_IR_NODE(Map)
PL_IR_NODE(Drop)
PL_IR_NODE(BlockingAgg)
PL_IR_NODE(Filter)
PL_IR_NODE(Limit)
PL_IR_NODE(GRPCSourceGroup)
PL_IR_NODE(GRPCSource)
PL_IR_NODE(GRPCSink)
PL_IR_NODE(Union)
PL_IR_NODE(Join)
PL_IR_NODE(TabletSourceGroup)
PL_IR_NODE(GroupBy)
PL_IR_NODE(UDTFSource)
PL_IR_NODE(Rolling)
PL_IR_NODE(Stream)
PL_IR_NODE(EmptySource)
PL_IR_NODE(OTelExportSink)

#endif
