#pragma once

#include <chrono>

namespace pl {
namespace stirling {

/**
 * A Sequence Generator Functor.
 *
 * @tparam T type of the values returned.
 */
template <typename T>
class Sequence {
 public:
  Sequence() {}

  virtual ~Sequence() = default;

  /**
   * Return next value in the sequence.
   *
   * @return T next value in the sequence.
   */
  virtual T operator()(void) = 0;

  /**
   * Resets sequence to its initial state.
   */
  virtual void Reset() {}
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
  explicit LinearSequence(T a, T b) : a_(a), b_(b) { Reset(); }

  /**
   * Return next value in the sequence.
   *
   * @return next value in the sequence.
   */
  T operator()(void) override {
    T val = a_ * x_ + b_;
    x_ += 1;
    return val;
  }

  /**
   * Reset the sequence.
   */
  void Reset() override { x_ = 0; }

 private:
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
  explicit QuadraticSequence(T a, T b, T c) : a_(a), b_(b), c_(c) { Reset(); }

  /**
   * Return next value in the sequence.
   *
   * @return next value in the sequence.
   */
  T operator()(void) override {
    T val = a_ * x_ * x_ + b_ * x_ + c_;
    x_ += 1;
    return val;
  }

  /**
   * Reset the sequence.
   */
  void Reset() override { x_ = 0; }

 private:
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
  FibonacciSequence() { Reset(); }

  /**
   * Return next value in the sequence.
   *
   * @return next value in the sequence.
   */
  T operator()(void) override {
    T val = fib_;
    fib_ = fibm1_ + fibm2_;
    fibm2_ = fibm1_;
    fibm1_ = fib_;
    return val;
  }

  /**
   * Reset the sequence.
   */
  void Reset() override {
    fib_ = 1;
    fibm1_ = 1;
    fibm2_ = 0;
  }

 private:
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
  explicit ModuloSequence(T n) : n_(n) { Reset(); }

  /**
   * Return next value in the sequence.
   *
   * @return next value in the sequence.
   */
  T operator()(void) override {
    T val = x_ % n_;
    x_ += 1;
    return val;
  }

  /**
   * Reset the sequence.
   */
  void Reset() override { x_ = 0; }

 private:
  T x_;
  T n_;
};

template <typename T>
class TimeSequence : public Sequence<T> {
 public:
  TimeSequence() { Reset(); }

  T operator()(void) override {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }

  void Reset() override {}
};

}  // namespace stirling
}  // namespace pl
