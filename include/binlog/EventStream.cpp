#include <binlog/EventStream.hpp>

#include <mserialize/deserialize.hpp>

namespace {

std::size_t readIstream(std::istream& input, void* dst, std::size_t size)
{
  input.read(static_cast<char*>(dst), std::streamsize(size));
  return std::size_t(input.gcount());
}

void rewindIstream(std::istream& input, std::size_t n)
{
  input.clear();
  input.seekg(-1 * std::streamsize(n), std::ios_base::cur);
}

} // namespace

namespace binlog {

const Event* EventStream::nextEvent(std::istream& input)
{
  while (true)
  {
    Range range = nextSizePrefixedRange(input);
    if (range.empty()) { return nullptr; }

    const std::uint64_t tag = range.read<std::uint64_t>();
    const bool special = (tag & (std::uint64_t(1) << 63)) != 0;

    if (special)
    {
      switch (tag)
      {
        case EventSource::Tag:
          readEventSource(range);
          break;
        case WriterProp::Tag:
          readWriterProp(range);
          break;
        case ClockSync::Tag:
          readClockSync(range);
          break;
        // default: ignore unkown special entries
        // to be forward compatible.
      }
    }
    else
    {
      readEvent(tag, range);
      return &_event;
    }
  }
}

Range EventStream::nextSizePrefixedRange(std::istream& input)
{
  std::uint32_t size = 0;
  const std::size_t readSize1 = readIstream(input, &size, sizeof(size));
  if (readSize1 == 0)
  {
    return {}; // eof
  }

  if (readSize1 != sizeof(size))
  {
    rewindIstream(input, readSize1);
    throw std::runtime_error("Failed to read range size from file, only got "
      + std::to_string(readSize1) + " bytes, expected " + std::to_string(sizeof(size)));
  }

  // TODO(benedek) protect agains bad alloc by limiting size?

  _buffer.resize(size);
  const std::size_t readSize2 = readIstream(input, _buffer.data(), size);

  if (readSize2 != size)
  {
    rewindIstream(input, readSize1 + readSize2);
    throw std::runtime_error("Failed to read range from file, only got "
      + std::to_string(readSize2) + " bytes, expected " + std::to_string(size));
  }

  return {_buffer.data(), _buffer.data() + size};
}

void EventStream::readEventSource(Range range)
{
  EventSource eventSource;
  mserialize::deserialize(eventSource, range);
  _eventSources[eventSource.id] = std::move(eventSource);
}

void EventStream::readWriterProp(Range range)
{
  // Make sure _writerProp is updated only if deserialize does not throw
  WriterProp wp;
  mserialize::deserialize(wp, range);
  _writerProp = std::move(wp);
}

void EventStream::readClockSync(Range range)
{
  // Make sure _clockSync is updated only if deserialize does not throw
  ClockSync clockSync;
  mserialize::deserialize(clockSync, range);
  _clockSync = std::move(clockSync);
}

void EventStream::readEvent(std::uint64_t eventSourceId, Range range)
{
  auto it = _eventSources.find(eventSourceId);
  if (it == _eventSources.end())
  {
    throw std::runtime_error("Event has invalid source id: " + std::to_string(eventSourceId));
  }

  _event.source = &it->second;
  _event.clockValue = range.read<std::uint64_t>();
  _event.arguments = range;
}

} // namespace binlog
