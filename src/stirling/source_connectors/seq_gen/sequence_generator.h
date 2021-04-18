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

#pragma once

#include <chrono>
#include <limits>
#include <string>
#include <vector>

#include <absl/strings/str_format.h>

#include "src/common/base/base.h"

namespace px {
namespace stirling {

/**
 * A Sequence Generator Functor.
 *
 * @tparam T type of the values returned.
 */
template <typename T>
class Sequence {
 public:
  Sequence() = default;

  virtual ~Sequence() = default;

  /**
   * Return next value in the sequence.
   *
   * @return T next value in the sequence.
   */
  virtual T operator()() = 0;

  /**
   * Resets sequence to its initial state.
   */
  virtual void Reset() { ResetImpl(); }

 protected:
  void ResetImpl() {}
};

/**
 * A Linear (y=a*x + b) Sequence Generator.
 *
 * @tparam T type of the values returned.
 */
template <typename T>
class LinearSequence : public Sequence<T> {
 public:
  /**
   * Constructor for a Linear Sequence (y=a*x+b).
   *
   * @param a linear equation slope.
   * @param b linear equation y-intercept.
   */
  explicit LinearSequence(T a, T b) : a_(a), b_(b) { ResetImpl(); }

  /**
   * Return next value in the sequence.
   *
   * @return next value in the sequence.
   */
  T operator()() override {
    T val = a_ * x_ + b_;
    x_ += 1;
    return val;
  }

  /**
   * Reset the sequence.
   */
  void Reset() override {
    Sequence<T>::ResetImpl();
    ResetImpl();
  }

 private:
  // Reset the sequence.
  void ResetImpl() { x_ = 0; }

  T x_;
  T a_;
  T b_;
};

/**
 * A Quadratic (y=a*x^2 + b*x + c) Sequence Generator.
 *
 * @tparam T type of the values returned.
 */
template <typename T>
class QuadraticSequence : public Sequence<T> {
 public:
  /**
   * Constructor for a Quadratic Sequence (y=a*x^2 + b*x + c).
   *
   * @param a quadratic co-efficient on x^2.
   * @param b quadratic co-efficient on x.
   * @param c y-intercept.
   */
  explicit QuadraticSequence(T a, T b, T c) : a_(a), b_(b), c_(c) { ResetImpl(); }

  /**
   * Return next value in the sequence.
   *
   * @return next value in the sequence.
   */
  T operator()() override {
    T val = a_ * x_ * x_ + b_ * x_ + c_;
    x_ += 1;
    return val;
  }

  /**
   * Reset the sequence.
   */
  void Reset() override {
    Sequence<T>::ResetImpl();
    ResetImpl();
  }

 private:
  // Reset the sequence.
  void ResetImpl() { x_ = 0; }

  T x_;
  T a_;
  T b_;
  T c_;
};

/**
 * A Fibonacci Sequence Generator.
 *
 * @tparam T type of the values returned.
 */
template <typename T>
class FibonacciSequence : public Sequence<T> {
 public:
  /**
   * Constructor for a Fibonacci Sequence.
   */
  FibonacciSequence() { ResetImpl(); }

  /**
   * Return next value in the sequence.
   *
   * @return next value in the sequence.
   */
  T operator()() override {
    T val = fib_;
    fib_ = fibm1_ + fibm2_;
    fibm2_ = fibm1_;
    fibm1_ = fib_;
    if (val > std::numeric_limits<T>::max() / 4) {
      Reset();
    }
    return val;
  }

  /**
   * Reset the sequence.
   */
  void Reset() override {
    Sequence<T>::ResetImpl();
    ResetImpl();
  }

 private:
  // Reset the sequence.
  void ResetImpl() {
    fib_ = 1;
    fibm1_ = 1;
    fibm2_ = 0;
  }

  T fib_;
  T fibm1_;
  T fibm2_;
};

/**
 * A Modulo Sequence Generator.
 *
 * @tparam T type of the values returned.
 */
template <typename T>
class ModuloSequence : public Sequence<T> {
 public:
  /**
   * Constructor for a Modulo Sequence.
   *
   * @param n the base of the modulo.
   */
  explicit ModuloSequence(T n) : n_(n) { ResetImpl(); }

  /**
   * Return next value in the sequence.
   *
   * @return next value in the sequence.
   */
  T operator()() override {
    T val = x_ % n_;
    x_ += 1;
    return val;
  }

  /**
   * Reset the sequence.
   */
  void Reset() override {
    Sequence<T>::ResetImpl();
    ResetImpl();
  }

 private:
  // Rest the sequence.
  void ResetImpl() { x_ = 0; }

  T x_;
  T n_;
};

template <typename T>
class TimeSequence : public Sequence<T> {
 public:
  TimeSequence() { ResetImpl(); }

  T operator()() override {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  void Reset() override {
    Sequence<T>::ResetImpl();
    ResetImpl();
  }

 private:
  // Reset the sequence.
  void ResetImpl() {}
};

/**
 * A String Sequence Generator.
 */
class StringSequence : public Sequence<std::string> {
 public:
  /**
   * Constructor for String Sequence.
   */
  StringSequence() {
    // Add line numbers to the lines.
    uint32_t line_number = 0;
    for (auto& line : text) {
      tokens.push_back(absl::StrFormat("%3d %s", line_number, line.data()));
      line_number++;
    }
    ResetImpl();
  }

  /**
   * Return next value in the sequence.
   *
   * @return next value in the sequence.
   */
  std::string operator()() override {
    std::string str = tokens[x_];

    x_ = (x_ + 1) % tokens.size();

    return str;
  }

  /**
   * Reset the sequence.
   */
  void Reset() override {
    Sequence<std::string>::ResetImpl();
    ResetImpl();
  }

 private:
  // Reset the sequence.
  void ResetImpl() { x_ = 0; }

  uint32_t x_ = 0;

  std::vector<std::string> tokens;

  static constexpr std::string_view text[] = {
      "To be, or not to be, that is the question:  ",
      "Whether 'tis nobler in the mind to suffer  ",
      "The slings and arrows of outrageous fortune,  ",
      "Or to take arms against a sea of troubles  ",
      "And by opposing end them. To die-to sleep,  ",
      "No more; and by a sleep to say we end  ",
      "The heart-ache and the thousand natural shocks  ",
      "That flesh is heir to: 'tis a consummation  ",
      "Devoutly to be wish'd. To die, to sleep;  ",
      "To sleep, perchance to dream-ay, there's the rub:  ",
      "For in that sleep of death what dreams may come,  ",
      "When we have shuffled off this mortal coil,  ",
      "Must give us pause-there's the respect  ",
      "That makes calamity of so long life.  ",
      "For who would bear the whips and scorns of time,  ",
      "Th'oppressor's wrong, the proud man's contumely,  ",
      "The pangs of dispriz'd love, the law's delay,  ",
      "The insolence of office, and the spurns  ",
      "That patient merit of th'unworthy takes,  ",
      "When he himself might his quietus make  ",
      "With a bare bodkin? Who would fardels bear,  ",
      "To grunt and sweat under a weary life,  ",
      "But that the dread of something after death,  ",
      "The undiscovere'd country, from whose bourn  ",
      "No traveller returns, puzzles the will,  ",
      "And makes us rather bear those ills we have  ",
      "Than fly to others that we know not of?  ",
      "Thus conscience does make cowards of us all,  ",
      "And thus the native hue of resolution  ",
      "Is sicklied o'er with the pale cast of thought,  ",
      "And enterprises of great pitch and moment  ",
      "With this regard their currents turn awry  ",
      "And lose the name of action. ",
  };
};

}  // namespace stirling
}  // namespace px
