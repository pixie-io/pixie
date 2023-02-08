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

import os
import argparse

import torch

from data.loader import build_loader
from model.model_factory import build_model
from model.metadata import kTargetProtocols
from utils.metrics import MeanPerClass
from utils.plot import plot_confusion_matrix
from utils.analysis import MisclassifiedExampleGenerator


def get_args():
    parser = argparse.ArgumentParser("Protocol Inference Benchmark")
    parser.add_argument("--batch", default=256, type=int)
    parser.add_argument("--output_dir", type=str)

    # Dataset Parameters.
    parser.add_argument("--dataset", type=str)
    parser.add_argument("--num_workers", default=8, type=int)

    # Model Parameters.
    parser.add_argument("--model", default="ruleset_basic", type=str)

    # Analysis Parameters.
    parser.add_argument("--analysis", action="store_true")
    parser.add_argument("--num_examples", default=10, type=int)

    args = parser.parse_args()
    return args


def test(loader, model, metric, analyzer):
    for batch_idx, (payloads, targets) in enumerate(loader):
        with torch.no_grad():
            preds = model(payloads)

        metric.update(targets, preds)
        if analyzer:
            analyzer.update(payloads, targets, preds)

        if batch_idx % 1000 == 0:
            print(f'batch:{batch_idx} | {metric}: {metric.avg()}')


def main():
    args = get_args()

    loader = build_loader(args)
    model = build_model(args)

    if args.analysis:
        analyzer = MisclassifiedExampleGenerator(
            os.path.join(args.output_dir, "misclassification.tsv"), k=args.num_examples)
    else:
        analyzer = None

    metric = MeanPerClass(num_classes=len(kTargetProtocols))
    test(loader, model, metric, analyzer)

    if analyzer:
        analyzer.close()

    print(f"{metric}: ", metric.avg())
    print(metric.confusion_matrix)
    plot_confusion_matrix(metric.confusion_matrix.numpy())


if __name__ == "__main__":
    main()
