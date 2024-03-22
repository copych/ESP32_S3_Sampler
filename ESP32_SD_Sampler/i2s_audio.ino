#include "driver/i2s.h"

//#define DEBUG_MASTER_OUT


const i2s_port_t i2s_num = I2S_NUM_0; // i2s port number

#ifdef USE_INTERNAL_DAC

void i2sInit() {
  pinMode(25, OUTPUT);
  pinMode(26, OUTPUT);

  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
    .sample_rate =  SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
    .dma_buf_count = DMA_NUM_BUF,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = false
  };

  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);
  i2s_set_dac_mode(I2S_DAC_CHANNEL_BOTH_EN);
  i2s_set_pin(i2s_num, NULL);
  i2s_zero_dma_buffer(i2s_num);
}
#else
void i2sInit() {
  pinMode(I2S_BCLK_PIN, OUTPUT);
  pinMode(I2S_DOUT_PIN, OUTPUT);
  pinMode(I2S_WCLK_PIN, OUTPUT);
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX ),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S ),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2,
    .dma_buf_count = DMA_NUM_BUF,
    .dma_buf_len = DMA_BUF_LEN,
    .use_apll = true,
  };

  i2s_pin_config_t i2s_pin_config = {
    .bck_io_num = I2S_BCLK_PIN,
    .ws_io_num =  I2S_WCLK_PIN,
    .data_out_num = I2S_DOUT_PIN
  };

  i2s_driver_install(i2s_num, &i2s_config, 0, NULL);

  i2s_set_pin(i2s_num, &i2s_pin_config);
  i2s_zero_dma_buffer(i2s_num);

  DEBF("I2S is started: BCK %d, WCK %d, DAT %d\r\n", I2S_BCLK_PIN, I2S_WCLK_PIN, I2S_DOUT_PIN);
}
#endif

void i2sDeinit() {
  i2s_zero_dma_buffer(i2s_num);
  i2s_driver_uninstall(i2s_num);
}



static inline void IRAM_ATTR i2s_output () {
// now out_buf is ready, output
size_t bytes_written;
#ifdef USE_INTERNAL_DAC
  for (int i=0; i < DMA_BUF_LEN; i++) {
    out_buf[out_buf_id][i*2] = (uint16_t)(127.0f * ( fast_shape( mix_buf_l[out_buf_id][i]) + 1.0f)) << 8U; // 256 output levels is way to little
    out_buf[out_buf_id][i*2+1] = (uint16_t)(127.0f * ( fast_shape( mix_buf_r[out_buf_id][i]) + 1.0f)) << 8U; // maybe you'll be lucky to fully use this range
  }
  i2s_write(i2s_num, out_buf[out_buf_id], sizeof(out_buf[out_buf_id]), &bytes_written, portMAX_DELAY);
#else
  for (int i=0; i < DMA_BUF_LEN; i++) {
    out_buf[out_buf_id][i*2] = (float)0x7fff * mix_buf_l[out_buf_id][i]; 
    out_buf[out_buf_id][i*2+1] = (float)0x7fff * mix_buf_r[out_buf_id][i];
   // if (i%4==0) DEBUG(out_buf[out_buf_id][i*2]);
  }
  i2s_write(i2s_num, out_buf[out_buf_id], sizeof(out_buf[out_buf_id]), &bytes_written, portMAX_DELAY);
#endif
}



static inline void IRAM_ATTR mixer() { // sum buffers 
#ifdef DEBUG_MASTER_OUT
  static float meter = 0.0f;
#endif
  static const float attenuator = 0.5f;
  static float sampler_out_l, sampler_out_r;
  static float mono_mix;

    for (int i=0; i < DMA_BUF_LEN; i++) {
      
      sampler_out_l = sampler_l[out_buf_id][i] * attenuator;
      sampler_out_r = sampler_r[out_buf_id][i] * attenuator;

// TODO: add fx 
      Reverb.Process(&sampler_out_l, &sampler_out_r);

      mix_buf_l[out_buf_id][i] = (sampler_out_l);
      mix_buf_r[out_buf_id][i] = (sampler_out_r);

      mono_mix = 0.5f * (mix_buf_l[out_buf_id][i] + mix_buf_r[out_buf_id][i]);

#ifdef DEBUG_MASTER_OUT
      if ( i % 16 == 0) meter = meter * 0.95f + fabs( mono_mix); 
#endif
      mix_buf_l[out_buf_id][i] = fclamp(mix_buf_l[out_buf_id][i] , -1.0f, 1.0f); // clipper
      mix_buf_r[out_buf_id][i] = fclamp(mix_buf_r[out_buf_id][i] , -1.0f, 1.0f);
 //    mix_buf_l[out_buf_id][i] = fast_shape( mix_buf_l[out_buf_id][i]); // soft limitter/saturator
 //    mix_buf_r[out_buf_id][i] = fast_shape( mix_buf_r[out_buf_id][i]);
   }
   
#ifdef DEBUG_MASTER_OUT
  meter *= 0.95f;
  meter += fabs(mono_mix); 
  DEBF("out= %0.5f\r\n", meter);
#endif
}

static inline void IRAM_ATTR sampler_generate_buf() {
  for (int i=0; i < DMA_BUF_LEN; i++){
    Sampler.getSample(sampler_l[gen_buf_id][i], sampler_r[gen_buf_id][i]) ;
  }
}
