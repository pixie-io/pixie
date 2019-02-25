#pragma once

#include <glog/logging.h>

#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "src/stirling/source_connector.h"

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
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  void Reset() override {}
};

/**
 * @brief Sequence Generator connector
 */
class SeqGenConnector : public SourceConnector {
 public:
  virtual ~SeqGenConnector() = default;

  static constexpr SourceType source_type = SourceType::kUnknown;

  static constexpr char kName[] = "sequence_generator";

  inline static const InfoClassSchema kElements = {
      InfoClassElement("_time", DataType::TIME64NS, Element_State::Element_State_SUBSCRIBED),
      InfoClassElement("x", DataType::INT64, Element_State::Element_State_SUBSCRIBED),
      InfoClassElement("x%10", DataType::INT64, Element_State::Element_State_SUBSCRIBED),
      InfoClassElement("x^2", DataType::INT64, Element_State::Element_State_SUBSCRIBED),
      InfoClassElement("Fib(x)", DataType::INT64, Element_State::Element_State_SUBSCRIBED),
      InfoClassElement("PI*x", DataType::FLOAT64, Element_State::Element_State_SUBSCRIBED)};

  static std::unique_ptr<SourceConnector> Create() {
    return std::unique_ptr<SourceConnector>(new SeqGenConnector());
  }

  void SetSeed(uint32_t seed) { rng_.seed(seed); }

  void ConfigureNumRowsPerGet(uint32_t min_num_rows, uint32_t max_num_rows) {
    num_rows_min_ = min_num_rows;
    num_rows_max_ = max_num_rows;
  }

  void ConfigureNumRowsPerGet(uint32_t num_rows) { ConfigureNumRowsPerGet(num_rows, num_rows); }

 protected:
  SeqGenConnector()
      : SourceConnector(source_type, kName, kElements),
        time_seq_(),
        lin_seq_(1, 1),
        mod10_seq_(10),
        square_seq_(1, 0, 0),
        pi_seq_(3.14159, 0),
        fib_seq_(),
        rng_(37) {}

  Status InitImpl() override { return Status::OK(); }

  RawDataBuf GetDataImpl() override;

  Status StopImpl() override { return Status::OK(); }

 private:
  TimeSequence<int64_t> time_seq_;
  LinearSequence<int64_t> lin_seq_;
  ModuloSequence<int64_t> mod10_seq_;
  QuadraticSequence<int64_t> square_seq_;
  LinearSequence<double> pi_seq_;
  FibonacciSequence<int64_t> fib_seq_;

  std::default_random_engine rng_;
  uint32_t num_rows_min_ = 0;
  uint32_t num_rows_max_ = 10;

  std::vector<uint8_t> data_buf_;
};

}  // namespace stirling
}  // namespace pl
