#ifndef BINLOG_SESSION_HPP
#define BINLOG_SESSION_HPP

#include <binlog/Entries.hpp>
#include <binlog/Severity.hpp>
#include <binlog/Time.hpp>
#include <binlog/detail/Queue.hpp>
#include <binlog/detail/QueueReader.hpp>
#include <binlog/detail/VectorOutputStream.hpp>

#include <atomic>
#include <cstdint>
#include <deque>
#include <list>
#include <mutex>
#include <utility> // move

namespace binlog {

/**
 * A concurrently writable and readable log stream.
 *
 * Session manages metadata (event sources, clock sync),
 * and data (log events). Members of this class
 * are thread-safe.
 *
 * Writers can add event sources and events.
 * Event sources are added directly, safe concurrent
 * access is ensured by a mutex.
 * Events can be added parallel, via Channels.
 * Channels wrap a single producer, lockfree queue.
 * The channel interface is raw, log events should be
 * added using SessionWriter.
 *
 * Readers can read metadata and data, via consume.
 * Concurrent reads are serialized by a mutex.
 *
 * Session responsibilities:
 *  - Assign unique ids to event sources
 *  - Add clock syncs to the stream when needed
 *  - Own data channels (lifetime management)
 *  - Ensure proper ordering of metadata and data,
 *    as observed by readers
 */
class Session
{
public:
  struct Channel
  {
    explicit Channel(std::size_t queueCapacity, WriterProp writerProp = {})
      :queue(queueCapacity),
       closed(false),
       writerProp(std::move(writerProp))
    {}

    detail::Queue queue;        /**< Holds log events */
    std::atomic<bool> closed;   /**< True, if queue will be no longer written */
    WriterProp writerProp;      /**< Describes the writer of this channel (optional) */
  };

  /** Describe the result of a consume call */
  struct ConsumeResult
  {
    std::size_t bytesConsumed = 0;      /**< Number of bytes written to the output stream by this call */
    std::size_t totalBytesConsumed = 0; /**< Total number of bytes written to the output stream in the lifetime of this session */
    std::size_t channelsPolled = 0;     /**< Number of channels polled to get log data from */
    std::size_t channelsRemoved = 0;    /**< Number of channels removed because they are empty and closed */
  };

  /**
   * Create a channel with a queue of `queueCapacity` bytes.
   *
   * Session retains ownership of the created channel.
   * The channel is disposed when it is marked closed
   * and is empty - by the next `consume` call.
   *
   * @return stable reference to the created channel
   */
  Channel& createChannel(std::size_t queueCapacity, WriterProp writerProp = {});

  /**
   * Thread-safe way to set the writer id of `channel` to `id`.
   *
   * @pre `channel` must be owned by *this
   * @post channel.writerProp.id == id
   */
  void setChannelWriterId(Channel& channel, std::uint64_t id);

  /**
   * Thread-safe way to set the writer name of `channel` to `name`.
   *
   * @pre `channel` must be owned by *this
   * @post channel.writerProp.name == name
   */
  void setChannelWriterName(Channel& channel, std::string name);

  /**
   * Add `eventSource` to the set of metadata managed by this session.
   *
   * The returned id can be used by event producers to
   * reference `eventSource` later in the stream.
   *
   * Events created after the addition of an EventSource
   * (addEventSource happens before addEvent)
   * are guaranteed to be consumed after the event source
   * by `Session::consume`.
   *
   * @returns the id assigned to the added event source
   */
  std::uint64_t addEventSource(EventSource eventSource);

  /** @returns Severity below writers should not add events */
  Severity minSeverity() const;

  /**
   * Set minimum severity new added events.
   *
   * This is advisory only: writers are encouraged
   * not to add new events with severity below the given limit,
   * but not required to.
   */
  void setMinSeverity(Severity severity);

  /**
   * Move metadata and data from the session to `out`.
   *
   * If needed (i.e: first time to consume), a
   * ClockSync is consumed, which describes std::chrono::system_clock.
   *
   * Then, metadata (EventSources) are consumed.
   * The consume logic makes sure sources are always consumed
   * sooner than events referencing them.
   *
   * After that, each channel is polled for log data,
   * and consumed together with an WriterProp entry, if data is found.
   * Closed and empty channels are removed.
   * Because data is consumed in batches, it is possible
   * that concurrently added events consumed from different channels
   * appear out of order. Events consumed from a single channel
   * are always in order.
   *
   * It is guaranteed that `out.write` always receives a sequence
   * of complete entries - no partial entries are written.
   *
   * @requires OutputStream must model the mserialize::OutputStream concept
   * @param out where the binary data will be written to.
   *
   * @returns description of the job done, see ConsumeResult.
   */
  template <typename OutputStream>
  ConsumeResult consume(OutputStream& out);

  /**
   * Move already consumed metadata again to `out`.
   *
   * Already consumed EventSources and a new ClockSync are consumed.
   * Not-yet consumed EventSources will not be consumed.
   *
   * Useful if `out` changes runtime, e.g: because of log rotation.
   * Re-adding metadata makes the new logfile self contained.
   *
   * @requires OutputStream must model the mserialize::OutputStream concept
   * @param out where the binary data will be written to.
   *
   * @returns description of the job done, see ConsumeResult.
   */
  template <typename OutputStream>
  ConsumeResult reconsumeMetadata(OutputStream& out);

private:
  template <typename Entry, typename OutputStream>
  std::size_t consumeSpecialEntry(const Entry& entry, OutputStream& out);

  std::mutex _mutex;

  std::list<Channel> _channels;
  std::deque<EventSource> _sources;
  std::size_t _numConsumedSources = 0;
  std::uint64_t _nextSourceId = 1;

  std::size_t _totalConsumedBytes = 0;

  std::atomic<Severity> _minSeverity = {Severity::trace};

  detail::VectorOutputStream _specialEntryBuffer;
};

inline Session::Channel& Session::createChannel(std::size_t queueCapacity, WriterProp writerProp)
{
  std::lock_guard<std::mutex> lock(_mutex);

  _channels.emplace_back(queueCapacity, std::move(writerProp));
  return _channels.back();
}

inline void Session::setChannelWriterId(Channel& channel, std::uint64_t id)
{
  std::lock_guard<std::mutex> lock(_mutex);

  channel.writerProp.id = id;
}

inline void Session::setChannelWriterName(Channel& channel, std::string name)
{
  std::lock_guard<std::mutex> lock(_mutex);

  channel.writerProp.name = std::move(name);
}

inline std::uint64_t Session::addEventSource(EventSource eventSource)
{
  std::lock_guard<std::mutex> lock(_mutex);

  eventSource.id = _nextSourceId;
  _sources.emplace_back(std::move(eventSource));
  return _nextSourceId++;
}

inline Severity Session::minSeverity() const
{
  return _minSeverity.load(std::memory_order_acquire);
}

inline void Session::setMinSeverity(Severity severity)
{
  _minSeverity.store(severity, std::memory_order_release);
}

template <typename OutputStream>
Session::ConsumeResult Session::consume(OutputStream& out)
{
  // This lock:
  //  - Ensures only a single consumer is running at a time
  //  - Ensures safe read of _channels
  //  - Ensures safe read of Channel::writerProp (written by setChannelWriterName)
  //  - Ensures safe read/write of _sources
  //  - Ensures no new EventSource can be added to _sources while consuming
  //
  // Without this lock, the following becomes possible:
  //  - Consumer starts, consumes _sources
  //  - Producer1 adds a new elem (ES123) to sources
  //  - Producer2 finds that ES123 is already added
  //  - Producer2 adds a new event using ES123
  //  - Consumer continues, consumes the event added by Producer2
  // This would result in a corrupt stream, as the event source
  // must precede every event referencing it. This is solved by the lock
  // that blocks P1 *and* P2 while adding ES123.
  std::lock_guard<std::mutex> lock(_mutex);

  ConsumeResult result;

  // add a clock sync to the beginning of the stream
  if (_totalConsumedBytes == 0)
  {
    const ClockSync clockSync = systemClockSync();
    result.bytesConsumed += consumeSpecialEntry(clockSync, out);
  }

  // consume event sources before events
  for (; _numConsumedSources < _sources.size(); ++_numConsumedSources)
  {
    result.bytesConsumed += consumeSpecialEntry(_sources[_numConsumedSources], out);
  }

  // consume some events
  for (auto it = _channels.begin(); it != _channels.end();)
  {
    // Important to check closed before beginRead,
    // otherwise the following race becomes possible:
    //  - Consumer finds queue is empty
    //  - Producer adds data
    //  - Producer closes the queue
    //  - Consumer finds queue is closed, removes it -> data loss
    const bool isClosed = it->closed;

    detail::QueueReader reader(it->queue);
    const detail::QueueReader::ReadResult data = reader.beginRead();
    if (data.size())
    {
      // consume writerProp entry
      it->writerProp.batchSize = data.size();
      result.bytesConsumed += consumeSpecialEntry(it->writerProp, out);

      // consume queue data
      out.write(data.buffer1, std::streamsize(data.size1));
      if (data.size2)
      {
        // data wraps around the end of the queue, consume the second half as well
        out.write(data.buffer2, std::streamsize(data.size2));
      }

      reader.endRead();
      result.bytesConsumed += data.size();
    }

    if (isClosed)
    {
      // queue is empty and closed, remove it
      it = _channels.erase(it);
      result.channelsRemoved++;
    }
    else
    {
      ++it;
    }

    result.channelsPolled++;
  }

  _totalConsumedBytes += result.bytesConsumed;
  result.totalBytesConsumed = _totalConsumedBytes;

  return result;
}

template <typename OutputStream>
Session::ConsumeResult Session::reconsumeMetadata(OutputStream& out)
{
  std::lock_guard<std::mutex> lock(_mutex);

  ConsumeResult result;

  // add clock sync
  const ClockSync clockSync = systemClockSync();
  result.bytesConsumed += serializeSizePrefixedTagged(clockSync, out);

  // add consumed sources
  for (std::size_t i = 0; i < _numConsumedSources; ++i)
  {
    result.bytesConsumed += serializeSizePrefixedTagged(_sources[i], out);
  }

  _totalConsumedBytes += result.bytesConsumed;
  result.totalBytesConsumed = _totalConsumedBytes;
  return result;
}

template <typename Entry, typename OutputStream>
std::size_t Session::consumeSpecialEntry(const Entry& entry, OutputStream& out)
{
  // Write entry to `_specialEntryBuffer` first, only then to `out` in one go.
  // This makes OutputStream logic simpler (if it parses the stream),
  // as it does not have to deal with partial entries.
  // (serializeSizePrefixedTagged serializes Entry field by field)
  // This is also more efficient if OutputStream does unbuffered I/O.
  _specialEntryBuffer.clear();
  const std::size_t size = serializeSizePrefixedTagged(entry, _specialEntryBuffer);
  out.write(_specialEntryBuffer.data(), _specialEntryBuffer.ssize());
  return size;
}

} // namespace binlog

#endif // BINLOG_SESSION_HPP
