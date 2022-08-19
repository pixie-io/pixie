# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

import re
from typing import List, Tuple
from faker import Faker
from dataclasses import dataclass
import dataclasses
import json


@dataclass()
class Span:
    """Span holds the start, end, value and type for a single (non-)PII template_name / entity."""
    value: str
    start: int
    end: int
    type: str

    def __repr__(self):
        return str(vars(self))


@dataclass()
class PayloadSpans:
    """PayloadSpans holds the labeled tokens for a single generated payload i.e. the full fake payload,
    the original template and a list of spans for each (non-)PII element."""
    fake: str
    spans: List[Span]
    template: str
    template_id: int

    def __repr__(self):
        return str(vars(self))

    @classmethod
    def from_json(cls, json_string):
        """Load PayloadSpans from a JSON string."""
        json_dict = json.loads(json_string)
        converted_spans = []
        for span_dict in json.loads(json_dict['spans']):
            converted_spans.append(PayloadSpans(**span_dict))
        json_dict['spans'] = converted_spans
        return cls(**json_dict)

    def to_json(self):
        """Serialize PayloadSpans to a JSON string."""
        payload_span = {
            "fake": self.fake,
            "spans": json.dumps([dataclasses.asdict(span) for span in self.spans]),
            "template": self.template,
            "template_id": self.template_id,
        }
        return json.dumps(payload_span)


class SpanGenerator(Faker):

    def __init__(self, locale="en_US", **kwargs):
        self._regex = re.compile(r"\{\{\s*(\w+)(:\s*\w+?)?\s*\}\}")
        # call init of Faker, passing given locale
        super().__init__(locale=locale, **kwargs)

    def parse(self, template: str, template_id: int) -> PayloadSpans:
        """Parse a payload template into a token-wise labeled span."""
        spans = []
        for match in self._regex.finditer(template):
            faker_attribute = match.group()[2:-2]
            spans.append(
                Span(
                    type=faker_attribute,
                    start=match.start(),
                    end=match.end(),
                    # format calls the Faker generator, producing a (non-)PII value
                    value=str(self.format(faker_attribute.strip())),
                )
            )
        spans.sort(reverse=True, key=lambda x: x.start)
        prev_end, payload_string = self.update_indices(spans, template)
        payload_string = f"{template[0:prev_end]}{payload_string}"
        return (
            PayloadSpans(
                fake=payload_string, spans=spans, template=template, template_id=template_id
            )
        )

    def update_indices(self, spans: List[Span], template: str) -> Tuple[int, str]:
        """Update span offsets given newly generated fake (non-PII) value which was
        inserted into the template"""
        payload_string = ""
        prev_end = len(template)
        for i, span in enumerate(spans):
            faker_attribute = span.type
            prev_len = len(faker_attribute) + 4
            new_len = len(f"{span.value}")
            payload_string = f"{template[span.end: prev_end]}{payload_string}"
            payload_string = f"{span.value}{payload_string}"
            prev_end = span.start
            len_difference = new_len - prev_len
            span.end += len_difference
            for j in range(0, i):
                spans[j].start += len_difference
                spans[j].end += len_difference
            span.type = faker_attribute.strip()
        return (prev_end, payload_string)
