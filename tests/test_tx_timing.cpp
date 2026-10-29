#include <QtTest>
#include <QFile>
#include <QDataStream>
#include <QDir>
#include <cstring>
#include <algorithm>
#include "TxTiming.hpp"
#include "Modulator/Modulator.hpp"
#include "Audio/soundout.h"
#include "commons.h"
#include "wsjtx_config.h"

int volatile itone[250] = {};
int volatile icw[250] = {};
// Noise is disabled for deterministic test transmissions.
float gran () {return 0;}
extern "C" {
  void genft8_ (char *, int *, int *, char *, char *, int *, fortran_charlen_t, fortran_charlen_t);
  void gen_ft8wave_ (int *, int *, int *, float *, float *, float *, float *, float *, int *, int *);
}

class RecordedModulator : public Modulator
{
public:
  explicit RecordedModulator (qint64 now) : Modulator {48000, 15.0}, now_ {now} {}
  qint64 render (char *data, qint64 size) {return readData (data, size);}
protected:
  qint64 currentTimeMs () const override {return now_;}
private:
  qint64 now_;
};

class TestTxTiming : public QObject
{
  Q_OBJECT
private:
  static QByteArray render (qint64 now, int advance, bool tune = false)
  {
    SoundOutput output; // No audio device: render PCM in memory only, never transmit.
    RecordedModulator modulator {now};
    modulator.tune (tune);
    modulator.start ("FT8", 79, 1920., 1500., -3., &output,
                     AudioDevice::Mono, true, false, 99., 15., advance);
    QByteArray result;
    for (int i = 0; i < 200 && modulator.isActive(); ++i) {
      char block[8192];
      auto size = modulator.render (block, sizeof block);
      if (!size) break;
      result.append (block, int(size));
      if (tune && result.size() >= 8192) break;
    }
    return result;
  }

  static void generateMessage (char const *message)
  {
    char input[37], sent[37], bits[77];
    std::memset (input, ' ', sizeof input);
    std::memcpy (input, message, std::strlen(message));
    int i3=0, n3=0;
    genft8_ (input, &i3, &n3, sent, bits, const_cast<int *>(itone), 37, 37);
    int symbols=79, samples=7680, complex=0, count=symbols*samples;
    float bt=2., rate=48000., frequency=1500.;
    gen_ft8wave_ (const_cast<int *>(itone), &symbols, &samples, &bt,
                  &rate, &frequency, foxcom_.wave, foxcom_.wave, &complex, &count);
  }

private Q_SLOTS:
  void boundaries_data ()
  {
    QTest::addColumn<qint64> ("period");
    QTest::addColumn<int> ("start");
    QTest::newRow("FT8") << qint64(15000) << 500;
    QTest::newRow("FT4") << qint64(7500) << 300;
    QTest::newRow("JT65") << qint64(60000) << 1000;
    QTest::newRow("WSPR") << qint64(120000) << 1000;
    QTest::newRow("FST4W") << qint64(1800000) << 1000;
    QTest::newRow("Echo") << qint64(3000) << 0;
  }
  void boundaries ()
  {
    QFETCH(qint64, period);
    QFETCH(int, start);
    for (qint64 boundary : {period, period*2, qint64(86400000)}) {
      QCOMPARE(TxTiming::phase(boundary-901, period, 900), period-1);
      QCOMPARE(TxTiming::phase(boundary-900, period, 900), qint64(0));
      QCOMPARE(TxTiming::phase(boundary-400, period, 900), qint64(500));
      QCOMPARE(TxTiming::phase(boundary, period, 0), qint64(0));
      // Advancing the clock must identify the upcoming even/odd slot.
      QCOMPARE(TxTiming::phase(boundary-900, 2*period, 900), boundary % (2*period));
      auto alignment = TxTiming::alignment(start, start, 48000, true);
      QCOMPARE(alignment.silentFrames, qint64(0));
      QCOMPARE(alignment.skippedFrames, qint64(0));
    }
  }

  void waveformAlignment ()
  {
    auto early=TxTiming::alignment(TxTiming::phase(14300, 15000, 900), 500, 48000, true);
    QCOMPARE(early.silentFrames, qint64(14400)); // 300 ms silence, audio at 14.600
    QCOMPARE(early.skippedFrames, qint64(0));
    auto onTime=TxTiming::alignment(TxTiming::phase(14600, 15000, 900), 500, 48000, true);
    QCOMPARE(onTime.silentFrames, qint64(0));
    QCOMPARE(onTime.skippedFrames, qint64(0));
    auto late=TxTiming::alignment(TxTiming::phase(14800, 15000, 900), 500, 48000, true);
    QCOMPARE(late.skippedFrames, qint64(9600));
    // Original silence/late-start behavior for zero advance throughout a slot.
    for (int ms=0; ms<15000; ++ms) {
      auto a=TxTiming::alignment(TxTiming::phase(ms, 15000, 0), 500, 48000, true);
      QCOMPARE(a.silentFrames, qint64(ms<500 ? (500-ms)*48 : 0));
      QCOMPARE(a.skippedFrames, qint64(ms>=500 ? (ms-500)*48 : 0));
    }
  }

  void settingsSnapshotAndPtt ()
  {
    TxTiming::Transmission tx;
    QCOMPARE(tx.selectedAdvanceMs(900, false), 900);
    QCOMPARE(tx.selectedAdvanceMs(900, true), 0);
    tx.begin(900, false);
    QCOMPARE(tx.selectedAdvanceMs(1500, false), 900);
    QCOMPARE(tx.advanceMs(), 900);
    QCOMPARE(tx.releaseDelayMs(200, true), 1100);
    QCOMPARE(tx.releaseDelayMs(0, true), 900); // TCI's original zero hold
    QCOMPARE(tx.releaseDelayMs(200, false), 200); // explicit Stop/Halt
    // Normal PTT release stays at its uncompensated deadline.
    QCOMPARE(13000-900+tx.releaseDelayMs(200, true), 13200);
    tx.released();
    QCOMPARE(tx.selectedAdvanceMs(1500, false), 1500);
    tx.begin(1500, true);
    QCOMPARE(tx.advanceMs(), 0);
    QCOMPARE(tx.releaseDelayMs(200, true), 200);
  }

  void recordedFt8 ()
  {
    // Compare complete real modulator output for two consecutive messages.
    // A 900 ms transport delay restores the baseline waveform's receive placement.
    char const *messages[] = {"CQ K1ABC FN42", "K1ABC W9XYZ -10"};
    for (int slot=1; slot<=2; ++slot) {
      generateMessage(messages[slot-1]);
      auto baseline=render(slot*15000+200, 0);
      auto advanced=render(slot*15000-700, 900);
      QVERIFY(baseline.size()>int(12.64*48000*2));
      QCOMPARE(advanced, baseline); // No lost opening or final symbols.
      auto samples=reinterpret_cast<qint16 const *>(advanced.constData());
      for (int i=0; i<14400; ++i) QCOMPARE(samples[i], qint16(0));
      QVERIFY(std::any_of(samples+14400, samples+15400, [](qint16 s) {return s != 0;}));
      // Save a standard 12 kHz mono WAV for an actual jt9 FT8 decode check.
      QByteArray pcm(15*12000*2, 0);
      for (int i=0; i<(advanced.size()/2)/4 && 2400+i<15*12000; ++i) {
        qint16 sample=samples[4*i];
        pcm[2*(2400+i)]=char(sample & 0xff);
        pcm[2*(2400+i)+1]=char((sample >> 8) & 0xff);
      }
      auto directory=qEnvironmentVariable("WSJT_TX_TEST_RECORDINGS");
      if (!directory.isEmpty()) {
        QFile file(QDir(directory).filePath(QString("000000_%1.wav").arg(slot*15,6,10,QChar('0'))));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QDataStream wav(&file);
        wav.setByteOrder(QDataStream::LittleEndian);
        wav.writeRawData("RIFF",4); wav << quint32(36+pcm.size());
        wav.writeRawData("WAVEfmt ",8); wav << quint32(16) << quint16(1) << quint16(1)
          << quint32(12000) << quint32(24000) << quint16(2) << quint16(16);
        wav.writeRawData("data",4); wav << quint32(pcm.size());
        wav.writeRawData(pcm.constData(),pcm.size());
      }
    }
  }

  void tuneUnaffected ()
  {
    QCOMPARE(render(14300, 900, true), render(14300, 0, true));
  }
};

QTEST_GUILESS_MAIN(TestTxTiming)
#include "test_tx_timing.moc"
