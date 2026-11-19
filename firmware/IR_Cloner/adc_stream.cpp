#include "adc_stream.h"
#include "config.h"
#include "esp_adc/adc_continuous.h"
#include "esp_timer.h"

namespace {
constexpr uint32_t FRAME_BYTES = 1024;   // 512 samples x 2 bytes per DMA frame
constexpr adc_channel_t CHANNEL = ADC_CHANNEL_7;   // GPIO35 (PIN_IR_RX)

adc_continuous_handle_t handle = nullptr;
uint32_t fsNominal = 0;
float fsMeasured = 0;
uint32_t fsMeasuredFor = 0;   // requested rate that fsMeasured belongs to
uint8_t frame[FRAME_BYTES];

float measure() {
  static uint16_t tmp[512];
  adcStream::flush();
  adcStream::read(tmp, 512, 100);                 // wait for the first fresh frame
  const int64_t t0 = esp_timer_get_time();
  uint32_t count = 0;
  while (esp_timer_get_time() - t0 < 400000) count += adcStream::read(tmp, 512, 100);
  const int64_t t1 = esp_timer_get_time();
  return count * 1e6f / float(t1 - t0);
}
}  // namespace

namespace adcStream {

bool start(uint32_t fs) {
  if (handle && fs == fsNominal) return true;
  stop();

  adc_continuous_handle_cfg_t hc = {};
  hc.max_store_buf_size = 16384;
  hc.conv_frame_size = FRAME_BYTES;
  if (adc_continuous_new_handle(&hc, &handle) != ESP_OK) {
    handle = nullptr;
    return false;
  }

  adc_digi_pattern_config_t pattern = {};
  pattern.atten = ADC_ATTEN_DB_12;                // ~0.15 .. 3.1 V input range
  pattern.channel = CHANNEL;
  pattern.unit = ADC_UNIT_1;
  pattern.bit_width = ADC_BITWIDTH_12;

  adc_continuous_config_t cfg = {};
  cfg.pattern_num = 1;
  cfg.adc_pattern = &pattern;
  cfg.sample_freq_hz = fs;
  cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE1;

  if (adc_continuous_config(handle, &cfg) != ESP_OK || adc_continuous_start(handle) != ESP_OK) {
    adc_continuous_deinit(handle);
    handle = nullptr;
    return false;
  }
  fsNominal = fs;
  if (fs != fsMeasuredFor) {   // the real rate only depends on the requested one: measure it once (0.4 s)
    fsMeasured = measure();
    fsMeasuredFor = fs;
    Serial.printf("ADC: asked %lu Sa/s, measured %.0f Sa/s\n", (unsigned long)fs, fsMeasured);
  }
  return true;
}

void stop() {
  if (!handle) return;
  adc_continuous_stop(handle);
  adc_continuous_deinit(handle);
  handle = nullptr;
  fsNominal = 0;
}

void flush() {
  // adc_continuous_flush_pool() only works while stopped, so drain the pool by reading it empty.
  if (!handle) return;
  uint32_t got = 0;
  for (int i = 0; i < 64; ++i) {
    if (adc_continuous_read(handle, frame, FRAME_BYTES, &got, 0) != ESP_OK || got == 0) break;
  }
}

uint32_t nominalFs() { return fsNominal; }
float measuredFs() { return fsMeasured; }

size_t read(uint16_t* out, size_t maxSamples, uint32_t timeoutMs) {
  if (!handle || maxSamples < 2) return 0;
  const uint32_t want = min<uint32_t>(maxSamples * 2, FRAME_BYTES) & ~3u;
  uint32_t got = 0;
  if (adc_continuous_read(handle, frame, want, &got, timeoutMs) != ESP_OK) return 0;

  size_t n = 0;
  for (uint32_t i = 0; i + 1 < got; i += 2) {
    const auto* d = reinterpret_cast<const adc_digi_output_data_t*>(&frame[i]);
    if (d->type1.channel == CHANNEL) out[n++] = d->type1.data;
  }
  if (ADC_SWAP_PAIRS) {
    for (size_t i = 0; i + 1 < n; i += 2) {
      const uint16_t t = out[i];
      out[i] = out[i + 1];
      out[i + 1] = t;
    }
  }
  return n;
}

}  // namespace adcStream
