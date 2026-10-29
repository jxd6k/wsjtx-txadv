#ifndef TX_TIMING_HPP
#define TX_TIMING_HPP

#include <QtGlobal>

// Only transmit scheduling uses this clock. Receive processing stays on UTC.
namespace TxTiming
{
  inline qint64 time (qint64 actualMs, int advanceMs)
  {
    return actualMs + advanceMs;
  }

  inline qint64 phase (qint64 actualMs, qint64 periodMs, int advanceMs)
  {
    auto remainder = time (actualMs, advanceMs) % periodMs;
    return remainder < 0 ? remainder + periodMs : remainder;
  }

  struct Alignment
  {
    qint64 silentFrames;
    qint64 skippedFrames;
  };

  inline Alignment alignment (qint64 phaseMs, unsigned nominalStartMs,
                               unsigned frameRate, bool synchronize)
  {
    return {synchronize && phaseMs < nominalStartMs
              ? (nominalStartMs - phaseMs) * frameRate / 1000 : 0,
            phaseMs >= nominalStartMs
              ? (phaseMs - nominalStartMs) * frameRate / 1000 : 0};
  }

  // Hold the selected advance from PTT assertion through PTT release, including
  // queued audio starts and waveform restarts after the settings dialog closes.
  class Transmission
  {
  public:
    int advanceMs () const {return advanceMs_;}
    int selectedAdvanceMs (int configuredMs, bool tuning) const
    {
      return active_ ? advanceMs_ : (tuning ? 0 : configuredMs);
    }
    void begin (int configuredMs, bool tuning)
    {
      advanceMs_ = tuning ? 0 : configuredMs;
      active_ = true;
    }
    int releaseDelayMs (int normalDelayMs, bool scheduled) const
    {
      return normalDelayMs + (scheduled ? advanceMs_ : 0);
    }
    void released () {active_ = false; advanceMs_ = 0;}

  private:
    int advanceMs_ = 0;
    bool active_ = false;
  };
}
#endif
