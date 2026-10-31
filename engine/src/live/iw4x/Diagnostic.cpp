#include "Diagnostic.hpp"

#include "Types.hpp"

// Adapted for cod4-live: diagnostics may originate on discovery threads.

namespace Controller
{
  const char*
  to_string (facility f) noexcept
  {
    switch (f)
    {
      case facility::runtime:     return "runtime";
      case facility::discovery:   return "discovery";
      case facility::transport:   return "transport";
      case facility::driver:      return "driver";
      case facility::decode:      return "decode";
      case facility::sample:      return "sample";
      case facility::calibration: return "calibration";
      case facility::mapping:     return "mapping";
      case facility::aim:         return "aim";
      case facility::steam:       return "steam";
      case facility::engine:      return "engine";
      case facility::debug:       return "debug";
    }

    return "runtime";
  }

  const char*
  to_string (severity s) noexcept
  {
    switch (s)
    {
      case severity::info:    return "info";
      case severity::warning: return "warning";
      case severity::error:   return "error";
    }

    return "info";
  }

  void
  logging_sink::
  consume (const diagnostic& d)
  {
    std::ostringstream line;
    line << "controller: " << to_string (d.origin) << ": " << d.message;

    if (d.code != errc::none)
      line << " [" << d.code << ']';

    if (d.device)
      line << ' ' << d.device;

    const std::string text = line.str () + "\n";
#ifdef _WIN32
    OutputDebugStringA (text.c_str ());
#endif
  }

  void
  report (diagnostic_sink& sink,
          severity level,
          facility origin,
          errc code,
          device_id device,
          std::string message)
  {
    sink.consume (diagnostic {level, origin, code, device, std::move (message)});
  }
}
