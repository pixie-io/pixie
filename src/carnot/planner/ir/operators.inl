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

#ifdef PX_CARNOT_IR_NODE

PX_CARNOT_IR_NODE(MemorySource)
PX_CARNOT_IR_NODE(MemorySink)
PX_CARNOT_IR_NODE(Map)
PX_CARNOT_IR_NODE(Drop)
PX_CARNOT_IR_NODE(BlockingAgg)
PX_CARNOT_IR_NODE(Filter)
PX_CARNOT_IR_NODE(Limit)
PX_CARNOT_IR_NODE(GRPCSourceGroup)
PX_CARNOT_IR_NODE(GRPCSource)
PX_CARNOT_IR_NODE(GRPCSink)
PX_CARNOT_IR_NODE(Union)
PX_CARNOT_IR_NODE(Join)
PX_CARNOT_IR_NODE(TabletSourceGroup)
PX_CARNOT_IR_NODE(GroupBy)
PX_CARNOT_IR_NODE(UDTFSource)
PX_CARNOT_IR_NODE(Rolling)
PX_CARNOT_IR_NODE(Stream)
PX_CARNOT_IR_NODE(EmptySource)
PX_CARNOT_IR_NODE(OTelExportSink)

#endif
