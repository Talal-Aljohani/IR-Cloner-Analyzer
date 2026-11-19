#include "ir_tx.h"

namespace {
constexpr uint32_t TICK_HZ = 1000000;            // 1 tick = 1 us
constexpr uint16_t MAX_TICKS = 32000;            // RMT duration field is 15 bits
rmt_data_t symbols[MAX_DURATIONS + 16];
uint32_t currentCarrier = 0;
bool ready = false;

// Sets the carrier and fills symbols[] (optional LOW lead-in, then mark/space...). Returns the symbol count.
size_t build(const IrSignal& sig, uint32_t leadUs, uint32_t& totalUs) {
  const uint32_t fc = (sig.carrierHz >= 30000 && sig.carrierHz <= 60000) ? sig.carrierHz : DEFAULT_CARRIER_HZ;
  if (fc != currentCarrier) {
    // carrier_level = false: the carrier is applied while the RMT level is HIGH (marks)
    if (!rmtSetCarrier(PIN_IR_TX, true, false, fc, CARRIER_DUTY)) {
      Serial.println("RMT: rmtSetCarrier failed");
      return 0;
    }
    currentCarrier = fc;
  }

  // Each RMT symbol holds two (level, duration) halves. Marks = HIGH, spaces = LOW.
  // Durations longer than 32 ms are split into several halves of the same level.
  size_t ns = 0;
  bool half = false;
  totalUs = leadUs;
  auto push = [&](bool level, uint32_t ticks) {
    while (ticks > 0 && ns < sizeof(symbols) / sizeof(symbols[0])) {
      const uint16_t t = min<uint32_t>(ticks, MAX_TICKS);
      ticks -= t;
      if (!half) {
        symbols[ns].level0 = level;
        symbols[ns].duration0 = t;
      } else {
        symbols[ns].level1 = level;
        symbols[ns].duration1 = t;
        ++ns;
      }
      half = !half;
    }
  };
  push(false, leadUs);
  for (int i = 0; i < sig.n; ++i) {
    push((i % 2) == 0, sig.us[i]);
    totalUs += sig.us[i];
  }
  push(false, 100);                         // short trailing space (a 0 duration would end the data early)
  if (half) push(false, 100);
  return ns;
}
}  // namespace

namespace irtx {

bool begin() {
  // GPIO27 HIGH = all 3 LEDs on at ~105 mA each: the 33R resistors would overheat. Force LOW first.
  pinMode(PIN_IR_TX, OUTPUT);
  digitalWrite(PIN_IR_TX, LOW);
  ready = rmtInit(PIN_IR_TX, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_2, TICK_HZ);
  if (ready) rmtSetEOT(PIN_IR_TX, LOW);
  else Serial.printf("RMT: rmtInit failed on GPIO%d\n", PIN_IR_TX);
  return ready;
}

bool send(const IrSignal& sig) {
  if (!ready || sig.n == 0) return false;
  uint32_t totalUs = 0;
  const size_t ns = build(sig, 0, totalUs);
  return ns && rmtWrite(PIN_IR_TX, symbols, ns, totalUs / 1000 + 200);
}

bool sendAsync(const IrSignal& sig, uint32_t leadUs) {
  if (!ready || sig.n == 0) return false;
  uint32_t totalUs = 0;
  const size_t ns = build(sig, leadUs, totalUs);
  return ns && rmtWriteAsync(PIN_IR_TX, symbols, ns);
}

bool busy() { return ready && !rmtTransmitCompleted(PIN_IR_TX); }

}  // namespace irtx
