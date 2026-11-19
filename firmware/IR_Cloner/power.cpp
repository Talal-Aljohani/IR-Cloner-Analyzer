#include "power.h"
#include "config.h"
#include "adc_stream.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

namespace {
constexpr adc_channel_t BAT_ADC_CHANNEL = ADC_CHANNEL_6;   // GPIO34 = PIN_BAT_ADC
uint32_t lastActivity = 0;
int level = -1;                  // backlight duty now (0..255)
int cachedMv = -1;               // smoothed battery voltage
int shownPct = -1;

void setLevel(int duty) {
  if (duty == level) return;
  level = duty;
  ledcWrite(PIN_TFT_BL, duty);
}

// ADC1 can run either the continuous stream (IR) or one-shot reads, not both: pause the stream meanwhile.
int readMv() {
  const uint32_t fs = adcStream::nominalFs();
  adcStream::stop();
  int mv = -1;
  adc_oneshot_unit_handle_t unit;
  adc_oneshot_unit_init_cfg_t ucfg = {};
  ucfg.unit_id = ADC_UNIT_1;
  if (adc_oneshot_new_unit(&ucfg, &unit) == ESP_OK) {
    adc_oneshot_chan_cfg_t cc = {};
    cc.atten = ADC_ATTEN_DB_12;
    cc.bitwidth = ADC_BITWIDTH_12;
    adc_oneshot_config_channel(unit, BAT_ADC_CHANNEL, &cc);
    adc_cali_handle_t cali = nullptr;
    adc_cali_line_fitting_config_t lc = {};
    lc.unit_id = ADC_UNIT_1;
    lc.atten = ADC_ATTEN_DB_12;
    lc.bitwidth = ADC_BITWIDTH_12;
    lc.default_vref = 1100;
    const bool haveCali = adc_cali_create_scheme_line_fitting(&lc, &cali) == ESP_OK;
    long sum = 0;
    int n = 0;
    for (int i = 0; i < 64; ++i) {
      int raw = 0, v = 0;
      if (adc_oneshot_read(unit, BAT_ADC_CHANNEL, &raw) != ESP_OK) continue;
      if (!haveCali || adc_cali_raw_to_voltage(cali, raw, &v) != ESP_OK) v = raw * 3100 / 4095;
      sum += v;
      ++n;
    }
    if (haveCali) adc_cali_delete_scheme_line_fitting(cali);
    adc_oneshot_del_unit(unit);
    if (n) mv = sum / n * BAT_DIVIDER;
  }
  if (fs) adcStream::start(fs);
  return mv;
}
}  // namespace

namespace power {

void begin() {
  ledcAttach(PIN_TFT_BL, 5000, 8);
  lastActivity = millis();
  setLevel(255);
}

void activity() {
  lastActivity = millis();
  setLevel(255);
}

bool asleep() { return level < 255; }

void update() {
  const uint32_t idle = millis() - lastActivity;
  if (idle > OFF_AFTER_MS) setLevel(0);
  else if (idle > DIM_AFTER_MS) setLevel(DIM_LEVEL);
}

int batteryMv(bool refresh) {
  if (refresh) {
    const int mv = readMv();
    // running average: the voltage moves with the load (backlight, IR LEDs), the charge does not
    if (mv > 0) cachedMv = cachedMv < 0 ? mv : (cachedMv * 3 + mv) / 4;
  }
  return cachedMv;
}

int batteryPercent() {
  if (cachedMv < 0) return -1;
  const int p = percent(cachedMv);
  if (shownPct < 0 || abs(p - shownPct) >= 2) shownPct = p;
  return shownPct;
}

int percent(int mv) {
  // resting voltage of a 1-cell Li-ion / LiPo against charge (approximate)
  static const int16_t V[] = {3300, 3500, 3600, 3700, 3750, 3800, 3900, 4000, 4100, 4200};
  static const int8_t P[] = {0, 8, 18, 32, 42, 52, 66, 79, 91, 100};
  if (mv <= V[0]) return 0;
  for (int i = 1; i < 10; ++i)
    if (mv <= V[i]) return P[i - 1] + (P[i] - P[i - 1]) * (mv - V[i - 1]) / (V[i] - V[i - 1]);
  return 100;
}

}  // namespace power
