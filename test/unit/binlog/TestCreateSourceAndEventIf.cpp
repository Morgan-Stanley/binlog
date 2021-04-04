#include <binlog/create_source_and_event_if.hpp>

#include "test_utils.hpp"

#include <binlog/Session.hpp>
#include <binlog/SessionWriter.hpp>
#include <binlog/Severity.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

namespace {

void logOnEveryLevel(binlog::SessionWriter& writer)
{
  BINLOG_CREATE_SOURCE_AND_EVENT_IF(writer, binlog::Severity::trace, category, 0, "");
  BINLOG_CREATE_SOURCE_AND_EVENT_IF(writer, binlog::Severity::debug, category, 0, "");
  BINLOG_CREATE_SOURCE_AND_EVENT_IF(writer, binlog::Severity::info, category, 0, "");
  BINLOG_CREATE_SOURCE_AND_EVENT_IF(writer, binlog::Severity::warning, category, 0, "");
  BINLOG_CREATE_SOURCE_AND_EVENT_IF(writer, binlog::Severity::error, category, 0, "");
  BINLOG_CREATE_SOURCE_AND_EVENT_IF(writer, binlog::Severity::critical, category, 0, "");
}

int failIfCalled()
{
  FAIL("Argument of disabled severity evaluated");
  return 0;
}

} // namespace

TEST_CASE("there_and_back_again")
{
  binlog::Session session;
  binlog::SessionWriter writer(session, 4096);

  // by default, every level is allowed
  logOnEveryLevel(writer);

  // disable trace, debug, info
  session.setMinSeverity(binlog::Severity::warning);
  logOnEveryLevel(writer);

  // disable every level
  session.setMinSeverity(binlog::Severity::no_logs);
  logOnEveryLevel(writer);

  // enable error, critical
  session.setMinSeverity(binlog::Severity::error);
  logOnEveryLevel(writer);

  // enable every level again
  session.setMinSeverity(binlog::Severity::trace);
  logOnEveryLevel(writer);

  const std::vector<std::string> expectedEvents{
      "TRAC", "DEBG", "INFO",   "WARN",   "ERRO", "CRIT",
    /*"TRAC", "DEBG", "INFO",*/ "WARN",   "ERRO", "CRIT",
    /*"TRAC", "DEBG", "INFO",   "WARN",   "ERRO", "CRIT",*/
    /*"TRAC", "DEBG", "INFO",   "WARN",*/ "ERRO", "CRIT",
      "TRAC", "DEBG", "INFO",   "WARN",   "ERRO", "CRIT",
  };

  CHECK(getEvents(session, "%S") == expectedEvents);
}

TEST_CASE("no_eval_if_disabled")
{
  binlog::Session session;
  binlog::SessionWriter writer(session, 128);

  session.setMinSeverity(binlog::Severity::warning);
  BINLOG_CREATE_SOURCE_AND_EVENT_IF(
    writer, binlog::Severity::info, category, 0, "{}", failIfCalled()
  );

  CHECK(true); // if reached, we are fine.
}
