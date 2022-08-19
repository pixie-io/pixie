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
import io
import pathlib
import argparse
import keras
import pandas as pd
import numpy as np
import tensorflow as tf
import matplotlib.pyplot as plt
import tensorflow_text as tf_text
import sentencepiece as spm
from sklearn.model_selection import train_test_split


class Dataset:
    def __init__(self, input_file):
        input_file = pathlib.Path(input_file)
        df = pd.read_csv(input_file, quotechar="|")
        df.rename(columns={0: "payload", 1: "has_pii", 2: "pii_types"}, inplace=True)
        self.df = df

    def train_test_split(self, test_size=0.2):
        df = self.df
        # stratify so that distribution of PII to non PII payloads is preserved in each of the splits
        self.X_train, self.X_test, y_train, y_test = train_test_split(
            df['payload'], df['has_pii'], stratify=df['has_pii'], test_size=test_size)
        self.y_train = np.asarray(y_train).astype('float32')
        self.y_test = np.asarray(y_test).astype('float32')
        print(
            f"proportion of full dataset that has PII: {sum(df.has_pii) / len(df)}")

    def __getitem__(self, index):
        return self.df[index]

    def __len__(self):
        return len(self.df)


class Tokenizer:
    def __init__(self, dataset, out_folder):
        self.dataset = dataset
        self.out_folder = pathlib.Path(out_folder)

    def train_sentence_piece(self, vocab_size=1024, max_num_of_inputs=500000):
        sp_model = io.BytesIO()
        # save text file containing training set to be used by sentencepiece
        np.savetxt(self.out_folder / "X_train.csv",
                   self.dataset.X_train, fmt='%s')
        # train sentencepiece model, creating "m.model" and "m.vocab"
        with open(self.out_folder / "X_train.csv", "r") as x_train:
            spm.SentencePieceTrainer.train(
                vocab_size=vocab_size, num_threads=64, input_sentence_size=max_num_of_inputs,
                model_writer=sp_model, sentence_iterator=x_train)
        # Serialize the model as file.
        with open(self.out_folder / 'sp.model', 'wb') as f:
            f.write(sp_model.getvalue())
        # wrap sentence piece segmeneter with tf_text
        sp_model = tf.io.gfile.GFile(self.out_folder / 'sp.model', 'rb').read()
        self.sentencepiece_tokenizer = tf_text.SentencepieceTokenizer(
            sp_model, out_type=tf.string)
        print(
            f"tokenizer has vocab size of {self.sentencepiece_tokenizer.vocab_size()}")

    def tokenize_test_set(self):
        # Tokenize the test set before passing as validation set in training
        self.dataset.X_test = self.sentencepiece_tokenizer.string_to_id(
            self.sentencepiece_tokenizer.tokenize(self.dataset.X_test))

    def tokenize_generator(self, tokenizer, X_train, y_train, batch_size):
        samples_per_epoch = X_train.shape[0]
        number_of_batches = samples_per_epoch / batch_size
        i = 0
        while (True):
            to_tokenize = X_train[batch_size * i:batch_size * (i + 1)]
            batch_x = tokenizer.string_to_id(tokenizer.tokenize(to_tokenize))
            batch_y = y_train[batch_size * i:batch_size * (i + 1)]
            i += 1
            yield (batch_x, batch_y)
            if i >= number_of_batches:
                i = 0


class LSTMBinary:
    def __init__(self, dataset, tokenizer, out_folder):
        self.dataset = dataset
        self.tokenizer = tokenizer
        self.out_folder = pathlib.Path(out_folder)

    def create_lstm_model(self, input_length=512):
        model = keras.models.Sequential()
        model.add(keras.Input(shape=(512, ), dtype=tf.int32))
        model.add(keras.layers.Embedding(
            int(self.tokenizer.sentencepiece_tokenizer.vocab_size()), 128, input_length=input_length))
        model.add(keras.layers.Dropout(0.5))
        model.add(keras.layers.LSTM(128, dropout=0.2))
        model.add(keras.layers.Dropout(0.5))
        model.add(keras.layers.Dense(1, activation='sigmoid'))
        model.compile(loss='binary_crossentropy',
                      optimizer='adam', metrics='accuracy')
        print(model.summary())
        return model

    def train(self, batch_size=128):
        # callback to reduce learning rate by factor of 0.2 if no improvement seen for 'patience' number of epochs
        reduce_lr = keras.callbacks.ReduceLROnPlateau(monitor='accuracy', factor=0.2,
                                                      patience=5, min_lr=0.001, verbose=1)
        # Stop early if no changes after 10 epochs, restoring the best weights
        early_stopping = keras.callbacks.EarlyStopping(patience=10,
                                                       restore_best_weights=True,
                                                       monitor='accuracy',
                                                       mode='max')
        self.model = self.create_lstm_model()
        self.history = self.model.fit(self.tokenizer.tokenize_generator(self.tokenizer.sentencepiece_tokenizer,
                                                                        self.dataset.X_train, self.dataset.y_train,
                                                                        batch_size),
                                      epochs=100, batch_size=batch_size,
                                      steps_per_epoch=self.dataset.X_train.shape[0] // batch_size,
                                      validation_steps=self.dataset.X_test.shape[0] // batch_size,
                                      callbacks=[early_stopping, reduce_lr],
                                      validation_data=(self.dataset.X_test, self.dataset.y_test))

    def save_model(self, model_path="lstm_binary.h5"):
        # save model
        model_path = self.out_folder / model_path
        self.model.save(model_path)

    def evaluate_on_testset(self, batch_size=128):
        score, acc = self.model.evaluate(self.dataset.X_test,
                                         np.asarray(self.dataset.y_test).astype('float32'),
                                         verbose=1, batch_size=batch_size)
        print(f"Model accuracy on test set: {round(acc * 100, 2)}%")

    def plot_history(self):
        # plot loss and accuracy
        pd.DataFrame(self.history.history).plot(figsize=(8, 5))
        plt.grid(True)
        plt.gca().set_ylim(0, 1)
        plt.show()

    def convert_to_tflite(self, input_length=512):
        model = self.model
        run_model = tf.function(lambda x: model(x))
        BATCH_SIZE = 1
        MAX_LENGTH = input_length
        concrete_func = run_model.get_concrete_function(
            tf.TensorSpec([BATCH_SIZE, MAX_LENGTH], model.inputs[0].dtype))
        model.save(self.out_folder, save_format="tf", signatures=concrete_func)
        converter = tf.lite.TFLiteConverter.from_saved_model(
            str(self.out_folder))
        tflite_model = converter.convert()
        with open(self.out_folder / "lstm_binary.tflite", "wb") as f:
            f.write(tflite_model)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Train LSTM model for Binary \"has PII\" classification",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    parser.add_argument(
        "--input",
        "-i",
        required=True,
        help="Absolute path to input data.",
    )

    parser.add_argument(
        "--out_folder",
        "-o",
        required=False,
        default=os.path.join(os.path.dirname(__file__), os.pardir),
        help="""Absolute path to where trained tf model.h5 will be stored.
        By default, saves to bazel cache for this runtime.""",
    )

    return parser.parse_args()


def main(args):
    # load dataset
    dataset = Dataset(args.input)
    dataset.train_test_split()

    # train sentencepiece tokenizer
    tokenizer = Tokenizer(dataset, args.out_folder)
    tokenizer.train_sentence_piece()
    tokenizer.tokenize_test_set()

    # train binary LSTM
    lstm = LSTMBinary(dataset, tokenizer, args.out_folder)
    lstm.train()
    lstm.save_model()
    lstm.plot_history()
    lstm.convert_to_tflite()
    lstm.evaluate_on_testset()


if __name__ == "__main__":
    args = parse_args()
    main(args)
