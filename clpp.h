// Copyright Mihai Preda.

#pragma once

#include "clwrap.h"
#include "AllocTrac.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <map>

template<typename T> class Buffer;

class Context : public std::unique_ptr<cl_context> {
  static constexpr unsigned BUF_CONST = CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR | CL_MEM_HOST_NO_ACCESS;

public:
  using std::unique_ptr<cl_context>::unique_ptr;
  
  template<typename T>
  auto constBuf(const std::vector<T>& vect, std::string_view name) const {
    return Buffer{*this, name, BUF_CONST, vect.size(), vect.data()};
  }
  
  template<typename T>
  auto hostAccessBuf(size_t size, std::string_view name) const {
    return Buffer<T>{*this, name, CL_MEM_READ_WRITE, size};
  }
  
  template<typename T>
  auto buffer(size_t size, std::string_view name) const {
    return Buffer<T>{*this, name, CL_MEM_READ_WRITE | CL_MEM_HOST_NO_ACCESS, size};
  }
};

template<typename T>
class Buffer : public std::unique_ptr<cl_mem> {
  size_t size_{};
  std::string name_;
  AllocTrac allocTrac;

  Buffer(cl_context context, std::string_view name, unsigned kind, size_t size, const T* ptr = nullptr)
    : std::unique_ptr<cl_mem>{makeBuf_(context, kind, size * sizeof(T), ptr)}
    , size_(size)
    , name_(name)
    , allocTrac(size * sizeof(T))
  {}
    
public:
  using type = T;
  
  Buffer() = default;

  Buffer(const Context& context, std::string_view name, unsigned kind, size_t size, const T* ptr = nullptr)
    : Buffer(context.get(), name, kind, size, ptr)
  {}

  size_t size() const { return size_; }
  const std::string& name() const { return name_; }
};

// Special-case Buffer argument: pass the wrapped cl_mem.
template<typename T>
void setArg(cl_kernel k, int pos, const Buffer<T>& buf) { setArg(k, pos, buf.get()); }

class Event : public EventHolder {
public:
  u64 nanos() { return getEventNanos(this->get()); }
};

struct TimeStats {
  u64 nanos{};
  u32 n{};
  void add(u64 delta) { nanos += delta; ++n; }
  void clear() { n = 0; nanos = 0; }
};

using QueuePtr = std::shared_ptr<class Queue>;

class Queue : public QueueHolder {
  // std::shared_ptr<QueueHolder> q;
  // using TimeMap = std::map<std::string, TimeStats>;
  // TimeMap timeMap;
  // std::vector<std::pair<Event, TimeMap::iterator>> events;
  // std::vector<Event> foo;


  // struct SharedAccess : public Queue { SharedAccess(cl_queue q) : Queue{q} {} };
public:

  explicit Queue(cl_queue q) : QueueHolder{q} {}  
  static QueuePtr makeQueue(cl_queue q) { return make_shared<Queue>(q); }
  

  /*
  Queue(const Queue&) = delete;
  void operator=(const Queue&) = delete;

  Queue(Queue&&) = default;
  Queue& operator=(Queue&&) = default;
  */
  
  template<typename T> vector<T> read(const Buffer<T>& buf, size_t sizeOrFull = 0) {
    auto size = sizeOrFull ? sizeOrFull : buf.size();
    assert(size <= buf.size());
    vector<T> ret(size);
    ::read(get(), true, buf.get(), size * sizeof(T), ret.data());
    return ret;
  }

  template<typename T> void readAsync(const Buffer<T>& buf, vector<T>& out, size_t sizeOrFull = 0) {
    auto size = sizeOrFull ? sizeOrFull : buf.size();
    assert(size <= buf.size());
    out.resize(size);
    ::read(get(), false, buf.get(), size * sizeof(T), out.data());
  }
    
  template<typename T> void write(Buffer<T>& buf, const vector<T> &vect) {
    assert(vect.size() <= buf.size());
    ::write(get(), true, buf.get(), vect.size() * sizeof(T), vect.data());
  }

  template<typename T> void writeAsync(Buffer<T>& buf, const vector<T> &vect) {
    assert(vect.size() <= buf.size());
    ::write(get(), false, buf.get(), vect.size() * sizeof(T), vect.data());
  }

  template<typename T> void copyFromTo(const Buffer<T>& src, Buffer<T>& dst) {
    assert(src.size() <= dst.size());
    copyBuf(get(), src.get(), dst.get(), src.size() * sizeof(T));
  }
  
  void run(cl_kernel kernel, size_t groupSize, size_t workSize, const string &name) {
    Event event{::run(get(), kernel, groupSize, workSize, name)};
    // events.emplace_back(std::move(event), timeMap.insert({name, TimeStats{}}).first);
  }

  void finish() {
    ::finish(get());
    // for (auto& [event, it] : events) { it->second.add(getEventNanos(event.get())); }
    // events.clear();
    
  }

  template<typename T> void zero(Buffer<T>& buf, size_t sizeOrFull = 0) {
    auto size = sizeOrFull ? sizeOrFull : buf.size();
    assert(size <= buf.size());
    T zero = 0;
    fillBuf(get(), buf.get(), &zero, sizeof(T), size * sizeof(T));
  }
};
