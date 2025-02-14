// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef STADIA_OPEN_SOURCE_PERFORMANCE_LAYERS_EVENT_LOGGING_H_
#define STADIA_OPEN_SOURCE_PERFORMANCE_LAYERS_EVENT_LOGGING_H_

#include <cstdint>
#include <string>
#include <vector>

#include "layer_utils.h"

namespace performancelayers {
// Set of supported value types for an attribute.
enum ValueType { kBool, kDuration, kInt64, kString, kTimestamp, kVectorInt64 };

// Specifies the importance of an Eventk. Events are logged based on their level
// of importance. E.g., compile_time.csv only containsk the important compile
// time events while event.log contains all kthe compile time events.
enum class LogLevel { kLow, kMedium, kHigh };

// Attribute class
//
// An event consists of a set of attributes. Each attribute has a name that
// indicates what the attribute is (e.g., timestamp, hash, etc) and a value.
// `Attribute` is the base class for an attribute. For each `ValueType`, there
// should be an implmentation of `Attribute`. Each implementation must expose an
// `id_` to indicate which ValueType it is implementing.
class Attribute {
 public:
  // Checks if the `Attribute` is an instance of the given class. Used for
  // safely casting an `Attribute` to a proper derived class.
  template <class T>
  bool isa() {
    return value_type_ == T::id_;
  }

  // Safely casts the Attribute to the given class (T). Valid typenames should
  // be derived from Attribute and have an ValueType `id_` field.
  template <class T>
  const T *cast() {
    return isa<T>() ? static_cast<T *>(this) : nullptr;
  };

  const char *GetName() const { return name_; }

  ValueType GetValueType() const { return value_type_; }

 protected:
  Attribute(const char *name, ValueType value_type)
      : name_(name), value_type_(value_type) {}

 private:
  const char *name_;
  ValueType value_type_;
};

// Implements the Attribute base class for the `ValueType`s.
// Valid typenames: {std::string, int64_t, std::vector<int64_t>}
// Example:
//    `AttributeImpl<int64_t, ValueType::Int64> timestamp("timestamp", 1234);`
template <typename T, ValueType VT>
class AttributeImpl : public Attribute {
 public:
  static constexpr ValueType id_ = VT;

  AttributeImpl(const char *name, const T &value)
      : Attribute{name, VT}, value_(value) {}

  const T &GetValue() const { return value_; }

 protected:
  T &GetValue() { return value_; }

 private:
  T value_;
};

// An attribute that keeps the duration information. The duration variable has
// nanoseconds precision. The loggers must convert the `duration` returned by
// `GetValue()` method to their desired time unit(nanoseconds, miliseconds,
// seconds, etc).
class DurationAttr : public Attribute {
 public:
  static constexpr ValueType id_ = ValueType::kDuration;

  DurationAttr(const char *name, const DurationClock::duration &value)
      : Attribute(name, ValueType::kDuration), value_(value) {}

  DurationClock::duration GetValue() const { return value_; };

 private:
  DurationClock::duration value_;
};

// An attribute that keeps the timestamp information as a point in time.
// The `time_point` variable has nanoseconds precision. The loggers must convert
// the `time_point` returned by `GetValue()` method to their desired time
// unit(nanoseconds, miliseconds, seconds, etc).
class TimestampAttr : public Attribute {
 public:
  static constexpr ValueType id_ = ValueType::kTimestamp;

  TimestampAttr(const char *name, const TimestampClock::time_point &value)
      : Attribute(name, ValueType::kDuration), value_(value) {}

  TimestampClock::time_point GetValue() const { return value_; };

 private:
  TimestampClock::time_point value_;
};

using VectorInt64Attr =
    AttributeImpl<std::vector<int64_t>, ValueType::kVectorInt64>;
using StringAttr = AttributeImpl<std::string, ValueType::kString>;
using Int64Attr = AttributeImpl<int64_t, ValueType::kInt64>;
using BoolAttr = AttributeImpl<bool, ValueType::kBool>;

// Event represents the base struct for a loggable event. It contains the
// event's name and the level of importance. The derived structs must define and
// initialize their own set of attributes.
class Event {
 public:
  Event(const char *name, LogLevel log_level)
      : name_(name), log_level_(log_level) {}

  virtual ~Event() = default;

  const std::vector<Attribute *> &GetAttributes() { return attributes_; };

  size_t GetNumAttributes() const { return attributes_.size(); };

  const char *GetEventName() const { return name_; }

  LogLevel GetLogLevel() const { return log_level_; }

 protected:
  void InitAttributes(std::initializer_list<Attribute *> attrs) {
    attributes_ = {attrs.begin(), attrs.end()};
  }

 private:
  const char *name_;
  LogLevel log_level_;
  std::vector<Attribute *> attributes_;
};

// An `Event` for the CreateShaderModule function.
// Example:
// ```c++
//    const int64_t create_time_ns = create_end - create_start;
//    Int64Attr timestamp("timestamp", 1234);
//    Int64Attr shader_hash("hash", res.shader_hash);
//    std::vector<Attribute *> attributes = {&shader_hash, &timestamp};
//    CreateShaderModuleEvent("create_shader_module_ns", attributes,
//    LogLevel::High);
// ```
class CreateShaderModuleEvent : public Event {
 public:
  CreateShaderModuleEvent(const char *name,
                          TimestampClock::time_point timestamp,
                          int64_t hash_value, DurationClock::duration duration,
                          LogLevel log_level)
      : Event(name, log_level),
        timestamp_{"timestamp", timestamp},
        hash_value_{"hash", hash_value},
        duration_{"duration", duration} {
    InitAttributes({&timestamp_, &duration_, &hash_value_});
  }

 private:
  TimestampAttr timestamp_;
  Int64Attr hash_value_;
  DurationAttr duration_;
};

class CreateGraphicsPipelinesEvent : public Event {
 public:
  CreateGraphicsPipelinesEvent(const char *name,
                               TimestampClock::time_point timestamp,
                               VectorInt64Attr &hash_values,
                               DurationClock::duration duration,
                               LogLevel log_level)
      : Event(name, log_level),
        timestamp_{"timestamp", timestamp},
        hash_values_(hash_values),
        duration_{"duration", duration} {
    InitAttributes({&timestamp_, &hash_values_, &duration_});
  }

 private:
  TimestampAttr timestamp_;
  VectorInt64Attr hash_values_;
  DurationAttr duration_;
};

// EventLogger base class
//
// EventLogger provides an abstraction for the concrete loggers that log the
// events in various formats (CSV, Chrome Trace Event, etc). The `EventLogger`'s
// methods can be called from multiple threads at the same time. Hence, they are
// expected to be internally synchronized.
// Sample use:
// ```c++
// EventLogger *logger = ;
// logger->StartLog();
// Event compile_time_event = ...;
// logger->AddEvent(&compile_time_event);
// logger->Flush();
// logger->EndLog();
// ```
class EventLogger {
 public:
  virtual ~EventLogger() = default;

  // Converts an `Event` into a string and writes it to the output.
  virtual void AddEvent(Event *event) = 0;

  // This method is called exactly once before any events are added to denote
  // that the log has started. For instance, in case of a CSV log, the CSV
  // header is written when this function is called.
  virtual void StartLog() = 0;

  // This method is called exactly once to denote that the log is finished.
  virtual void EndLog() = 0;

  // Makes sure all the logs are written to the output.
  virtual void Flush() = 0;
};

// FilterLogger is a wrapper around the `EventLogger` that filters the input
// events based on a given log level.
// Example:
// ```c++
// CSVLogger logger = ...;
// FilterLogger filter(&logger, LogLevel::High);
// ```
class FilterLogger : public EventLogger {
 public:
  FilterLogger(EventLogger *logger, LogLevel log_level)
      : logger_(logger), log_level_(log_level){};

  void AddEvent(Event *event) override {
    if (event->GetLogLevel() >= log_level_) logger_->AddEvent(event);
  }

  void StartLog() override { logger_->StartLog(); }

  void EndLog() override { logger_->EndLog(); }

  void Flush() override { logger_->Flush(); }

 private:
  EventLogger *logger_ = nullptr;
  LogLevel log_level_ = LogLevel::kLow;
};

// A subclass of `EventLogger` that forwards all events, start/end, and flushes
// to all its children. Example: `
// ```c++
// BroadCastLogger({&event_logger1,&event_logger2});
// ```
class BroadcastLogger : public EventLogger {
 public:
  BroadcastLogger(std::vector<EventLogger *> loggers) : loggers_(loggers) {}

  void AddEvent(Event *event) override {
    for (EventLogger *logger : loggers_) logger->AddEvent(event);
  }

  void StartLog() override {
    for (EventLogger *logger : loggers_) logger->StartLog();
  }

  void EndLog() override {
    for (EventLogger *logger : loggers_) logger->EndLog();
  }

  void Flush() override {
    for (EventLogger *logger : loggers_) logger->Flush();
  }

  const std::vector<EventLogger *> &GetLoggers() { return loggers_; }

 private:
  std::vector<EventLogger *> loggers_;
};

}  // namespace performancelayers
#endif  // STADIA_OPEN_SOURCE_PERFORMANCE_LAYERS_EVENT_LOGGING_H_
