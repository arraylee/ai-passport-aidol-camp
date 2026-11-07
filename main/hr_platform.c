#include "esp_partition.h"
#include "hr_platform.h"
#include "esp_heap_caps.h"
#include "bsp_audio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "hr_ble.h"
#include "hr_cards.h"
#include "hr_preview.h"
#include "hr_identity.h"
#include "hr_edit.h"
#include "hr_profile_store.h"
#include "hr_social.h"
#include "hr_welcome.h"
#include "hr_game.h"
#include "hr_sfx.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#define CHANNEL 6
static uint32_t ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
typedef struct {
  hr_packet_t p;
  uint8_t target[6], speaker[6];
  uint8_t mac[6];
  int8_t rssi;
  uint32_t at;
} rx_t;
typedef struct {
  uint8_t data[HR_AUDIO];
  uint32_t gate, at;
} audio_t;
static QueueHandle_t rx_queue, tx_audio, rx_audio;
static atomic_bool sending, audio_stop, welcome_done, game_capture;
static atomic_uint game_hz, game_hz_at, game_mic_stack, game_rms, game_raw_hz;
static int16_t game_pcm[640];
static hr_sfx_bus_t game_sound;
static atomic_uint welcome_request;
static atomic_uint audio_ack;
static TaskHandle_t capture_handle, playback_handle;
static atomic_uint capture_count, play_count, mic_peak;
static atomic_uint drops, audio_errors, tx_gate, rx_gate, volume;
static atomic_uint rx_kind[HR_KIND_MAX + 1], tx_kind[HR_KIND_MAX + 1];
static uint32_t send_at, gate_serial, old_tx_token, old_tx_request,
    old_rx_epoch, old_rx_token, last_audio;
static uint8_t old_rx_speaker;
static bool radio_ready, audio_ready, storage_ready;
static void receive_cb(const esp_now_recv_info_t *info, const uint8_t *data,
                       int n) {
  if (!info || !info->rx_ctrl || !info->src_addr || !rx_queue)
    return;
  rx_t r = {.at = ms(), .rssi = info->rx_ctrl->rssi};
  hr_identity_frame_t frame;
  if (!hr_identity_unpack(&frame, data, (size_t)n))
    return;
  r.p=frame.p;memcpy(r.target,frame.target,6);memcpy(r.speaker,frame.speaker,6);
  atomic_fetch_add(&rx_kind[r.p.kind], 1);
  memcpy(r.mac, info->src_addr, 6);
  if (xQueueSend(rx_queue, &r, 0) != pdTRUE)
    atomic_fetch_add(&drops, 1);
}
static void send_cb(const esp_now_send_info_t *info,
                    esp_now_send_status_t status) {
  (void)info;
  if (status != ESP_NOW_SEND_SUCCESS)
    atomic_fetch_add(&drops, 1);
  atomic_store(&sending, false);
}
bool hr_radio_init(uint8_t mac[6]) {
  if (esp_netif_init() != ESP_OK)
    return false;
  esp_err_t e = esp_event_loop_create_default();
  if (e != ESP_OK && e != ESP_ERR_INVALID_STATE)
    return false;
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  if (esp_wifi_init(&cfg) != ESP_OK ||
      esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK ||
      esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
      esp_wifi_start() != ESP_OK ||
      esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE) != ESP_OK ||
      esp_wifi_set_ps(WIFI_PS_NONE) != ESP_OK ||
      esp_wifi_get_mac(WIFI_IF_STA, mac) != ESP_OK)
    return false;
  rx_queue = xQueueCreate(24, sizeof(rx_t));
  if (!rx_queue)
    return false;
  if (esp_now_init() != ESP_OK ||
      esp_now_register_recv_cb(receive_cb) != ESP_OK ||
      esp_now_register_send_cb(send_cb) != ESP_OK)
    return false;
  esp_now_peer_info_t peer = {
      .channel = CHANNEL, .ifidx = WIFI_IF_STA, .encrypt = false};
  memset(peer.peer_addr, 255, 6);
  if (esp_now_add_peer(&peer) != ESP_OK)
    return false;
  radio_ready = true;
  return true;
}
static void send_packet(const hr_core_t *c,const hr_packet_t *p) {
  uint8_t wire[HR_IDENTITY_WIRE_MAX];
  size_t len = hr_identity_pack(c,p,wire);
  if(!len)return;
  static const uint8_t all[6] = {255, 255, 255, 255, 255, 255};
  atomic_store(&sending, true);
  send_at = ms();
  esp_err_t send_error=esp_now_send(all,wire,len);
  if(send_error!=ESP_OK) {
    static uint32_t error_at=0;
    if(!error_at || (uint32_t)(ms()-error_at)>5000) {
      error_at=ms();ESP_LOGW("radio","send=%s dma=%u largest=%u",
      esp_err_to_name(send_error),(unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    }
    atomic_store(&sending, false);
    atomic_fetch_add(&drops, 1);
  } else
    atomic_fetch_add(&tx_kind[p->kind], 1);
}
void hr_radio_diagnostics(void) {
  ESP_LOGI("radio",
           "tx hello=%u request=%u grant=%u voice=%u; "
           "rx hello=%u request=%u grant=%u voice=%u",
           atomic_load(&tx_kind[HR_HELLO]), atomic_load(&tx_kind[HR_REQUEST]),
           atomic_load(&tx_kind[HR_GRANT]), atomic_load(&tx_kind[HR_VOICE]),
           atomic_load(&rx_kind[HR_HELLO]), atomic_load(&rx_kind[HR_REQUEST]),
           atomic_load(&rx_kind[HR_GRANT]), atomic_load(&rx_kind[HR_VOICE]));
  ESP_LOGI("cards_radio","tx meta=%u req=%u data=%u; rx meta=%u req=%u data=%u",
    atomic_load(&tx_kind[HR_CARD_META]),atomic_load(&tx_kind[HR_CARD_REQUEST]),atomic_load(&tx_kind[HR_CARD_DATA]),
    atomic_load(&rx_kind[HR_CARD_META]),atomic_load(&rx_kind[HR_CARD_REQUEST]),atomic_load(&rx_kind[HR_CARD_DATA]));
}

void hr_audio_gate(hr_core_t *c, uint32_t now, bool enabled) {
  bool tx = enabled && audio_ready && hr_can_talk(c, now);
  if (tx) {
    if (!atomic_load(&tx_gate) || c->token != old_tx_token ||
        c->request != old_tx_request) {
      old_tx_token = c->token;
      old_tx_request = c->request;
      atomic_store(&tx_gate, ++gate_serial);
    }
  } else
    atomic_store(&tx_gate, 0);
  bool rx = enabled && !c->game_busy && c->hardware_ok && hr_room_member(c, c->id, now) &&
            hr_room_member(c, c->host, now) && hr_room_member(c, c->speaker, now) &&
            audio_ready && c->speaker && c->speaker != c->id &&
            !hr_conflict(c, c->id, now) && !hr_conflict(c, c->host, now) &&
            (c->floor_target == 0 || c->floor_target == c->id);
  if (rx) {
    if (!atomic_load(&rx_gate) || c->epoch != old_rx_epoch ||
        c->token != old_rx_token || c->speaker != old_rx_speaker) {
      old_rx_epoch = c->epoch;
      old_rx_token = c->token;
      old_rx_speaker = c->speaker;
      atomic_store(&rx_gate, ++gate_serial);
    }
  } else
    atomic_store(&rx_gate, 0);
}
static void profile_received(hr_core_t *c, const rx_t *r) {
  if (r->p.kind != HR_PROFILE || r->p.id == c->id)
    return;
  hr_peer_t *q = &c->peers[r->p.id];
  hr_edit_t edited;
  if (q->seq == r->p.seq && q->boot == r->p.boot && q->last == r->at &&
      !memcmp(q->mac, r->mac, 6) && hr_edit_wire_decode(&edited, r->p.audio))
    hr_profile_remote(r->p.id, edited.name, edited.intro, edited.avatar);
}
static bool profile_packet(hr_core_t *c, hr_packet_t *p, uint32_t now) {
  static uint32_t at;
  if (!c->id || c->held || c->speaker || (uint32_t)(now - at) < 2000)
    return false;
  at = now;
  *p = (hr_packet_t){
      .kind = HR_PROFILE, .id = c->id, .boot = c->boot, .seq = ++c->seq};
  hr_edit_wire_encode(hr_profile_current(), p->audio);
  return true;
}
void hr_radio_poll(hr_core_t *c, uint32_t now) {
  if (!radio_ready)
    return;
  now = ms();
  hr_identity_elect(c,now);
  hr_tick(c, now);
  rx_t r;
  for (int n = 0; n < 24 && xQueueReceive(rx_queue, &r, 0) == pdTRUE; n++) {
    // A callback may run after main sampled now, or during this queue drain.
    now = ms();
    if ((uint32_t)(now - r.at) > 180) {
      atomic_fetch_add(&drops, 1);
      continue;
    }
    hr_identity_frame_t frame={.p=r.p};
    memcpy(frame.target,r.target,6);memcpy(frame.speaker,r.speaker,6);
    uint8_t before[HR_PLAYERS+1][6];
    for(unsigned i=1;i<=HR_PLAYERS;i++)memcpy(before[i],c->peers[i].mac,6);
    bool routed=hr_identity_route(c,&frame,r.mac,r.at);
    for(unsigned i=1;i<=HR_PLAYERS;i++)if(memcmp(before[i],c->peers[i].mac,6)){
      hr_profile_clear(i);hr_profile_remote(i,"设备","",false);
      ESP_LOGI("identity","peer slot=%u MAC=%02x:%02x:%02x:%02x:%02x:%02x",i,r.mac[0],r.mac[1],r.mac[2],r.mac[3],r.mac[4],r.mac[5]);
    }
    if(!routed)continue;
    r.p=frame.p;
    if (hr_receive(c, &r.p, r.mac, r.rssi, r.at) && audio_ready) {
      hr_audio_gate(c, now, true);
      audio_t a = {.gate = atomic_load(&rx_gate), .at = r.at};
      memcpy(a.data, r.p.audio, HR_AUDIO);
      if (a.gate && xQueueSend(rx_audio, &a, 0) == pdTRUE)
        last_audio = now;
      else
        atomic_fetch_add(&drops, 1);
    }
    profile_received(c, &r);
    hr_peer_t *q = &c->peers[r.p.id];
    if (!hr_ble_upload_busy() && !hr_ble_connected() && !c->held &&
        !c->speaker && r.p.kind >= HR_CARD_META && q->seq == r.p.seq &&
        q->boot == r.p.boot && q->last == r.at && !memcmp(q->mac, r.mac, 6))
    {
      if(r.p.kind>=HR_PREVIEW_REQUEST && r.p.kind<=HR_PREVIEW_DATA) hr_preview_receive(c,&r.p,now);
      else if(r.p.kind<=HR_CARD_DATA) hr_cards_receive(c, &r.p, now);
    }
  }
  now = ms();
  hr_identity_elect(c,now);
  hr_tick(c, now);
  hr_audio_gate(c, now, true);
  // Fail closed if the Wi-Fi callback stalls: do not submit overlapping frames.
  if (atomic_load(&sending)) {
    if ((uint32_t)(now - send_at) > 1000) {
      c->hardware_ok = false;
      atomic_store(&tx_gate, 0);
    }
    return;
  }
  hr_packet_t p;
  if (hr_control(c, &p, now)) {
    send_packet(c,&p);
    return;
  }
  if (!hr_ble_upload_busy() && !hr_ble_connected() &&
      hr_cards_send(c, &p, now)) {
    send_packet(c,&p);
    return;
  }
  if (!hr_ble_upload_busy() && !hr_ble_connected() &&
      hr_preview_send(c, &p, now)) {
    send_packet(c,&p);
    return;
  }
  static uint32_t score_at;
  if ((uint32_t)(now-score_at)>=1000 && hr_score_packet(c,&p,now)) {
    score_at=now;send_packet(c,&p);return;
  }
  if (profile_packet(c, &p, now)) {
    send_packet(c,&p);
    return;
  }
  audio_t a;
  if (audio_ready && xQueueReceive(tx_audio, &a, 0) == pdTRUE) {
    now = ms();
    if (a.gate && a.gate == atomic_load(&tx_gate) &&
        (uint32_t)(now - a.at) < 80 && hr_voice(c, &p, a.data, now))
      send_packet(c,&p);
    else
      atomic_fetch_add(&drops, 1);
  }
}
static void audio_barrier(unsigned bit) {
  if (!atomic_load(&audio_stop))
    return;
  while (atomic_load(&audio_stop)) {
    atomic_fetch_or(&audio_ack, bit);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  atomic_fetch_and(&audio_ack, ~bit);
}
static void capture_task(void *unused) {
  (void)unused;
  int16_t pcm[HR_SAMPLES];
  audio_t a;
  unsigned game_used=0,history[3]={0},history_at=0,windows=0;
  hr_voice_gate_t voice={0};
  for (;;) {
    audio_barrier(1);
    uint32_t gate = atomic_load(&tx_gate);
    if (bsp_audio_read(pcm, sizeof pcm) != ESP_OK) {
      atomic_fetch_add(&audio_errors, 1);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if(hr_sfx_guarded(&game_sound,ms())) {
      game_used=0;history_at=0;memset(history,0,sizeof history);
      voice=(hr_voice_gate_t){0};atomic_store(&game_rms,0);atomic_store(&game_raw_hz,0);
      atomic_store(&game_hz,0);atomic_store(&game_hz_at,0);
      continue; // Drain mic DMA, discard speaker echo and all pitch history.
    }
    if(atomic_load(&game_capture)) {
      memcpy(game_pcm+game_used,pcm,sizeof pcm);game_used+=HR_SAMPLES;
      if(game_used==640) {
        unsigned hz=hr_voice_hz(&voice,game_pcm);atomic_store(&game_rms,voice.level);
        atomic_store(&game_raw_hz,voice.candidate_hz);
        if(!voice.open){memset(history,0,sizeof history);history_at=0;}
        history[history_at++%3]=hz;
        unsigned a0=history[0],b0=history[1],c0=history[2],t;
        if(a0>b0){t=a0;a0=b0;b0=t;}if(b0>c0){t=b0;b0=c0;c0=t;}if(a0>b0)b0=a0;
        atomic_store(&game_hz,b0);atomic_store(&game_hz_at,ms());game_used=0;
        if(++windows%25==0)atomic_store(&game_mic_stack,(unsigned)uxTaskGetStackHighWaterMark(NULL));
      }
      continue; // Game microphone is exclusive; never enqueue radio voice.
    }
    game_used=0;memset(history,0,sizeof history);voice=(hr_voice_gate_t){0};
    if (gate && gate == atomic_load(&tx_gate)) {
      a.gate = gate;
      unsigned peak = 0;
      for (unsigned i = 0; i < HR_SAMPLES; i++) {
        int x = pcm[i];
        unsigned u = x < 0 ? -x : x;
        if (u > peak)
          peak = u;
      }
      atomic_store(&mic_peak, peak);
      atomic_fetch_add(&capture_count, 1);
      a.at = ms();
      hr_adpcm_encode(pcm, a.data);
      if (xQueueSend(tx_audio, &a, 0) != pdTRUE)
        atomic_fetch_add(&drops, 1);
    }
    // I2S read is the 20 ms capture clock, including while idle (no stale
    // microphone DMA).
  }
}
static void playback_task(void *unused) {
  (void)unused;
  int16_t pcm[HR_SAMPLES];
  audio_t a;
  uint32_t active = 0;
  bool started = false;
  unsigned previous = 101;
  hr_welcome_t welcome = {0};
  hr_sfx_player_t sound={0};
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    audio_barrier(2);
    uint32_t gate = atomic_load(&rx_gate);
    if (gate != active) {
      active = gate;
      started = false;
    }
    memset(pcm, 0, sizeof pcm);
    if (atomic_load(&welcome_request) == 1) {
      if (hr_welcome_begin(&welcome, hr_welcome_data, hr_welcome_size))
        atomic_store(&welcome_request, 2);
      else
        atomic_store(&welcome_request, 0);
    }
    bool welcoming = atomic_load(&welcome_request) == 2;
    if (welcoming) {
      (void)hr_welcome_next(&welcome, pcm);
    } else if (gate && (started || uxQueueMessagesWaiting(rx_audio) >= 3)) {
      started = true;
      while (xQueueReceive(rx_audio, &a, 0) == pdTRUE) {
        if (a.gate == gate && (uint32_t)(ms() - a.at) < 160) {
          atomic_fetch_add(&play_count, 1);
          if (!hr_adpcm_decode(a.data, pcm))
            atomic_fetch_add(&audio_errors, 1);
          break;
        }
      }
    } else if (!gate) {
      while (xQueueReceive(rx_audio, &a, 0) == pdTRUE) {
      }
    }
    if(atomic_load(&game_capture)) {
      unsigned kind=sound.kind;
      bool sounding=hr_sfx_frame(&game_sound,&sound,pcm,HR_SAMPLES,ms());
      if(kind && sounding && !sound.kind)ESP_LOGI("game","sfx=%u completed",kind);
    } else hr_sfx_begin(&sound,HR_SFX_NONE);
    unsigned v = atomic_load(&volume);
    if (v != previous) {
      bsp_audio_set_volume(v);
      previous = v;
    }
    bool written = bsp_audio_write(pcm, sizeof pcm) == ESP_OK;
    if (!written)
      atomic_fetch_add(&audio_errors, 1);
    if (welcoming && (!written || welcome.position == welcome.samples)) {
      atomic_store(&welcome_done, written);
      atomic_store(&welcome_request, 0);
      ESP_LOGI("intro", "welcome audio completed=%u samples=%u", written,
               (unsigned)welcome.position);
    }
    vTaskDelayUntil(&wake, pdMS_TO_TICKS(20));
  }
}
bool hr_audio_pause(void) {
  hr_sfx_enable(&game_sound,false,ms());
  atomic_store(&welcome_request, 0);
  if (!audio_ready && !capture_handle && !playback_handle && !tx_audio &&
      !rx_audio) {
    bsp_audio_deinit();
    return true;
  }
  atomic_store(&tx_gate, 0);
  atomic_store(&rx_gate, 0);
  atomic_store(&audio_ack, 0);
  atomic_store(&audio_stop, true);
  unsigned wanted = (capture_handle ? 1 : 0) | (playback_handle ? 2 : 0);
  uint32_t at = ms();
  while ((atomic_load(&audio_ack) & wanted) != wanted && ms() - at < 1200)
    vTaskDelay(pdMS_TO_TICKS(5));
  if ((atomic_load(&audio_ack) & wanted) != wanted) {
    atomic_store(&audio_stop, false);
    ESP_LOGE("audio", "stop timed out; peripherals retained");
    return false;
  }
  if (capture_handle)
    vTaskDelete(capture_handle);
  if (playback_handle)
    vTaskDelete(playback_handle);
  capture_handle = playback_handle = NULL;
  audio_ready = false;
  bsp_audio_deinit();
  if (tx_audio)
    vQueueDelete(tx_audio);
  if (rx_audio)
    vQueueDelete(rx_audio);
  tx_audio = rx_audio = NULL;
  return true;
}
bool hr_audio_init(void) {
  if (audio_ready)
    return true;
  if (capture_handle || playback_handle)
    return false;
  if (bsp_audio_init() != ESP_OK ||
      bsp_audio_set_format(16000, 16, 1) != ESP_OK) {
    bsp_audio_deinit();
    return false;
  }
  atomic_store(&audio_stop, false);
  atomic_store(&audio_ack, 0);
  tx_audio = xQueueCreate(4, sizeof(audio_t));
  rx_audio = xQueueCreate(8, sizeof(audio_t));
  if (!tx_audio || !rx_audio) {
    hr_audio_pause();
    return false;
  }
  if (xTaskCreate(capture_task, "mic", 4096, NULL, 5, &capture_handle) !=
          pdPASS ||
      xTaskCreate(playback_task, "speaker", 4096, NULL, 5, &playback_handle) !=
          pdPASS) {
    hr_audio_pause();
    return false;
  }
  audio_ready = true;
  return true;
}
bool hr_radio_pause(void) {
  if (!radio_ready)
    return true;
  radio_ready = false;
  if (esp_wifi_stop() == ESP_OK)
    return true;
  radio_ready = true;
  return false;
}
void hr_radio_resume(void) {
  if (!radio_ready && esp_wifi_start() == ESP_OK) {
    esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
    atomic_store(&sending, false);
    xQueueReset(rx_queue);
    radio_ready = true;
  }
}
void hr_audio_volume(uint8_t value) {
  atomic_store(&volume, value > 100 ? 100 : value);
}
unsigned hr_radio_drops(void) { return atomic_load(&drops); }
unsigned hr_audio_errors(void) { return atomic_load(&audio_errors); }
bool hr_audio_recent(uint32_t now) {
  return last_audio && (uint32_t)(now - last_audio) < 350;
}
bool hr_storage_load(uint8_t *id, uint8_t *v) {
  // Only the explicitly assigned demo partition may be formatted. Original
  // nvs/cardid and all other partitions are never opened for writing.
  esp_err_t e = nvs_flash_init_partition("demo_cfg");
  if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW("storage", "formatting only demo_cfg 0x360000+0x6000");
    e = nvs_flash_erase_partition("demo_cfg");
    if (e == ESP_OK)
      e = nvs_flash_init_partition("demo_cfg");
  }
  storage_ready = e == ESP_OK;
  if (!storage_ready) {
    ESP_LOGE("storage", "demo_cfg unavailable: %s; preserved",
             esp_err_to_name(e));
    return false;
  }
  nvs_handle_t h;
  if (nvs_open_from_partition("demo_cfg", "hideseek", NVS_READONLY, &h) !=
      ESP_OK)
    return false;
  uint8_t b[12];
  size_t n = sizeof b;
  e = nvs_get_blob(h, "settings", b, &n);
  nvs_close(h);
  return e == ESP_OK && n == sizeof b && hr_config_decode(b, id, v);
}
bool hr_storage_save(uint8_t id, uint8_t volume_value) {
  if (!storage_ready)
    return false;
  nvs_handle_t h;
  if (nvs_open_from_partition("demo_cfg", "hideseek", NVS_READWRITE, &h) !=
      ESP_OK)
    return false;
  uint8_t b[12];
  hr_config_encode(id, volume_value, b);
  esp_err_t e = nvs_set_blob(h, "settings", b, sizeof b);
  if (e == ESP_OK)
    e = nvs_set_u8(h, "volume90_v1", 1);
  if (e == ESP_OK)
    e = nvs_commit(h);
  nvs_close(h);
  return e == ESP_OK;
}

bool hr_storage_apply_volume_default(uint8_t id, uint8_t *value) {
  if (!storage_ready || !value || id < 1 || id > HR_PLAYERS)
    return false;
  nvs_handle_t h;
  if (nvs_open_from_partition("demo_cfg", "hideseek", NVS_READONLY, &h) != ESP_OK)
    return false;
  uint8_t initialized = 0;
  esp_err_t e = nvs_get_u8(h, "volume90_v1", &initialized);
  nvs_close(h);
  if (e == ESP_OK && initialized == 1)
    return true;
  // Settings and migration marker share one commit. Later user choices stay.
  if (!hr_storage_save(id, 90))
    return false;
  *value = 90;
  return true;
}

void hr_audio_stats(unsigned *captured, unsigned *played, unsigned *peak) {
  *captured = atomic_load(&capture_count);
  *played = atomic_load(&play_count);
  *peak = atomic_load(&mic_peak);
}
bool hr_storage_friends_load(uint8_t id, uint8_t *mask) {
  if (!storage_ready)
    return false;
  nvs_handle_t h;
  if (nvs_open_from_partition("demo_cfg", "hideseek", NVS_READONLY, &h) !=
      ESP_OK)
    return false;
  char key[16];
  snprintf(key, sizeof key, "friends_%u", id);
  uint8_t b[12];
  size_t n = sizeof b;
  esp_err_t e = nvs_get_blob(h, key, b, &n);
  nvs_close(h);
  return e == ESP_OK && n == sizeof b && hr_friends_decode(id, b, mask);
}
bool hr_storage_friends_save(uint8_t id, uint8_t mask) {
  if (!storage_ready)
    return false;
  nvs_handle_t h;
  if (nvs_open_from_partition("demo_cfg", "hideseek", NVS_READWRITE, &h) !=
      ESP_OK)
    return false;
  char key[16];
  snprintf(key, sizeof key, "friends_%u", id);
  uint8_t b[12];
  hr_friends_encode(id, mask, b);
  esp_err_t e = nvs_set_blob(h, key, b, sizeof b);
  if (e == ESP_OK)
    e = nvs_commit(h);
  nvs_close(h);
  return e == ESP_OK;
}

bool hr_audio_paused(void) {
  return !audio_ready && !capture_handle && !playback_handle;
}

void hr_audio_welcome_start(void) {
  atomic_store(&welcome_done, false);
  if (audio_ready)
    atomic_store(&welcome_request, 1);
}
bool hr_audio_welcome_done(void) { return atomic_load(&welcome_done); }
bool hr_storage_welcomed(void) {
  nvs_handle_t h;
  if (!storage_ready || nvs_open_from_partition("demo_cfg", "hideseek", NVS_READONLY, &h) != ESP_OK)
    return false;
  uint8_t value = 0;
  esp_err_t e = nvs_get_u8(h, "welcome_v2", &value);
  nvs_close(h);
  return e == ESP_OK && value == 1;
}
bool hr_storage_welcome_save(void) {
  nvs_handle_t h;
  if (!storage_ready || nvs_open_from_partition("demo_cfg", "hideseek", NVS_READWRITE, &h) != ESP_OK)
    return false;
  esp_err_t e = nvs_set_u8(h, "welcome_v2", 1);
  if (e == ESP_OK) e = nvs_commit(h);
  nvs_close(h);
  return e == ESP_OK;
}

bool hr_storage_activated(void) {
  nvs_handle_t h;
  if (!storage_ready || nvs_open_from_partition("demo_cfg", "hideseek", NVS_READONLY, &h) != ESP_OK)
    return false;
  uint8_t value = 0;
  esp_err_t e = nvs_get_u8(h, "activated_v1", &value);
  nvs_close(h);
  return e == ESP_OK && value == 1;
}
bool hr_storage_activation_save(void) {
  nvs_handle_t h;
  if (!storage_ready || nvs_open_from_partition("demo_cfg", "hideseek", NVS_READWRITE, &h) != ESP_OK)
    return false;
  esp_err_t e = nvs_set_u8(h, "activated_v1", 1);
  if (e == ESP_OK) e = nvs_commit(h);
  nvs_close(h);
  return e == ESP_OK;
}

void hr_audio_game(bool enabled) {
  if(!enabled)hr_sfx_enable(&game_sound,false,ms());
  if(enabled!=atomic_load(&game_capture)) {
    atomic_store(&game_hz,0);atomic_store(&game_hz_at,0);atomic_store(&game_mic_stack,0);
    atomic_store(&tx_gate,0);atomic_store(&rx_gate,0);
    atomic_store(&game_capture,enabled);
  }
}
unsigned hr_audio_pitch(void) {
  if(hr_sfx_guarded(&game_sound,ms()))return 0;
  return atomic_load(&game_capture) && (uint32_t)(ms()-atomic_load(&game_hz_at))<180 ? atomic_load(&game_hz):0;
}
unsigned hr_audio_mic_stack(void) {return atomic_load(&game_mic_stack);}
unsigned hr_audio_game_rms(void) {return atomic_load(&game_rms);}
unsigned hr_audio_game_raw_pitch(void) {return atomic_load(&game_raw_hz);}
bool hr_storage_best_load(unsigned id,uint32_t *score) {
  if(!storage_ready)return false;
  nvs_handle_t h;if(nvs_open_from_partition("demo_cfg","hideseek",NVS_READONLY,&h)!=ESP_OK)return false;
  uint8_t b[16];size_t n=sizeof b;esp_err_t e=nvs_get_blob(h,"best_v1",b,&n);nvs_close(h);
  unsigned decoded=0;
  bool valid=e==ESP_OK && n==sizeof b && hr_best_decode(id,b,&decoded);
  if(valid)*score=decoded;
  return valid;
}
bool hr_storage_best_save(unsigned id,unsigned score) {
  if(!storage_ready || id<1 || id>HR_PLAYERS || score>HR_SCORE_MAX)return false;
  nvs_handle_t h;if(nvs_open_from_partition("demo_cfg","hideseek",NVS_READWRITE,&h)!=ESP_OK)return false;
  uint8_t b[16];hr_best_encode(id,score,b);
  esp_err_t e=nvs_set_blob(h,"best_v1",b,sizeof b);if(e==ESP_OK)e=nvs_commit(h);
 nvs_close(h);return e==ESP_OK;
}

void hr_audio_game_sounds(const hr_game_t *game,bool enabled,uint32_t now) {
  static hr_sfx_watch_t watch;
  hr_sfx_enable(&game_sound,enabled,now);
  unsigned cue=hr_sfx_event(&watch,game,enabled,now);
  if(cue) {
    hr_sfx_request(&game_sound,cue,now);
    atomic_store(&game_hz,0);atomic_store(&game_hz_at,0);
    ESP_LOGI("game","sfx=%u queued",cue);
  }
}

bool hr_storage_roster_load(hr_core_t *c) {
  if(!storage_ready)return false;
  nvs_handle_t h;if(nvs_open_from_partition("demo_cfg","hideseek",NVS_READONLY,&h)!=ESP_OK)return false;
  uint8_t b[HR_ROSTER_BYTES];size_t n=sizeof b;
  esp_err_t e=nvs_get_blob(h,"roster_mac_v1",b,&n);nvs_close(h);
  return e==ESP_OK&&n==sizeof b&&hr_roster_decode(c,b);
}
bool hr_storage_roster_save(const hr_core_t *c,uint8_t friends) {
  if(!storage_ready||!c->id)return false;
  nvs_handle_t h;esp_err_t opened=nvs_open_from_partition("demo_cfg","hideseek",NVS_READWRITE,&h);
  if(opened!=ESP_OK){ESP_LOGW("cards_store","roster open=%s",esp_err_to_name(opened));return false;}
  uint8_t b[HR_ROSTER_BYTES];hr_roster_encode(c,b);b[5]=friends;
  uint32_t crc=hr_crc(b,50);for(unsigned i=0;i<4;i++)b[50+i]=(uint8_t)(crc>>(8*i));
  esp_err_t e=nvs_set_blob(h,"roster_mac_v1",b,sizeof b);
  if(e==ESP_OK)e=nvs_commit(h);
  nvs_close(h);if(e!=ESP_OK)ESP_LOGW("cards_store","roster commit=%s",esp_err_to_name(e));return e==ESP_OK;
}

/* Bluetooth management only touches these named, exact demo data partitions. */
bool hr_storage_config_mark(uint8_t role,uint8_t action,uint32_t txn) {
 nvs_handle_t h;uint8_t b[12]={'H','B','C','1',role,action,0,0};
 memcpy(b+8,&txn,4);
 if(nvs_open_from_partition("demo_cfg","ble_config",NVS_READWRITE,&h)!=ESP_OK)return false;
 esp_err_t e=nvs_set_blob(h,"pending",b,sizeof b);if(e==ESP_OK)e=nvs_commit(h);
 nvs_close(h);return e==ESP_OK;
}
bool hr_storage_config_pending(uint8_t *role,uint8_t *action,uint32_t *txn) {
 nvs_handle_t h;uint8_t b[12];size_t n=sizeof b;
 if(nvs_open_from_partition("demo_cfg","ble_config",NVS_READONLY,&h)!=ESP_OK)return false;
 esp_err_t e=nvs_get_blob(h,"pending",b,&n);nvs_close(h);
 if(e!=ESP_OK||n!=sizeof b||memcmp(b,"HBC1",4))return false;
 *role=b[4];*action=b[5];memcpy(txn,b+8,4);return true;
}
bool hr_storage_config_clear(void) {
 nvs_handle_t h;if(nvs_open_from_partition("demo_cfg","ble_config",NVS_READWRITE,&h)!=ESP_OK)return false;
 esp_err_t e=nvs_erase_key(h,"pending");if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;
 if(e==ESP_OK)e=nvs_commit(h);
 nvs_close(h);return e==ESP_OK;
}
bool hr_storage_config_reset(uint8_t role) {
 if(!storage_ready||role<1||role>6)return false;
 nvs_handle_t h;if(nvs_open_from_partition("demo_cfg","hideseek",NVS_READWRITE,&h)!=ESP_OK)return false;
 uint8_t b[12];hr_config_encode(role,90,b);
 esp_err_t e=nvs_erase_all(h);
 if(e==ESP_OK)e=nvs_set_blob(h,"settings",b,sizeof b);
 if(e==ESP_OK)e=nvs_set_u8(h,"volume90_v1",1);
 if(e==ESP_OK)e=nvs_set_u8(h,"volume100_v1",1);
 if(e==ESP_OK)e=nvs_commit(h);
 nvs_close(h);return e==ESP_OK;
}
bool hr_storage_config_erase(unsigned region,uint32_t offset) {
 static const char *names[]={"profile_store","friend_store","gallery_store"};
 static const uint32_t addresses[]={0x380000,0x3b0000,0x400000},sizes[]={0x30000,0x30000,0xc0000};
 if(region>=3||offset%4096||offset>=sizes[region])return false;
 const esp_partition_t*p=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,0x41+region,names[region]);
 return p&&p->address==addresses[region]&&p->size==sizes[region]&&esp_partition_erase_range(p,offset,4096)==ESP_OK;
}
