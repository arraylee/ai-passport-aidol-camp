#include <string.h>
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_sleep.h"
#include "esp_attr.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hr_ble.h"
#include "hr_cards.h"
#include "hr_preview.h"
#include "hr_controls.h"
#include "hr_gallery.h"
#include "hr_platform.h"
#include "hr_power.h"
#include "hr_profile_store.h"
#include "hr_ui.h"
#include "hr_touch.h"
#include "hr_identity.h"
static hr_view_t view;
static RTC_NOINIT_ATTR struct {
  uint32_t magic, boots, sleeps, sleep_boot, non_usb_reset, non_usb_wake;
} sleep_trace;
static void trace_boot(void) {
  int reset = esp_reset_reason(), wake = esp_sleep_get_wakeup_cause();
  if (sleep_trace.magic != 0x48525332u) {
    sleep_trace = (typeof(sleep_trace)){.magic = 0x48525332u};
  }
  ++sleep_trace.boots;
  if (reset != ESP_RST_USB) {
    sleep_trace.non_usb_reset = reset;
    sleep_trace.non_usb_wake = wake;
  }
}
static void trace_report(void) {
  ESP_LOGI("power", "retained boots=%lu sleeps=%lu sleep_boot=%lu last_non_usb_reset=%lu last_non_usb_wake=%lu current_reset=%d current_wake=%d",
    (unsigned long)sleep_trace.boots, (unsigned long)sleep_trace.sleeps,
    (unsigned long)sleep_trace.sleep_boot, (unsigned long)sleep_trace.non_usb_reset,
    (unsigned long)sleep_trace.non_usb_wake,
    (int)esp_reset_reason(), (int)esp_sleep_get_wakeup_cause());
}
void hr_power_record_sleep(void) {
  ++sleep_trace.sleeps;
  sleep_trace.sleep_boot = sleep_trace.boots;
  trace_report();
}

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static bool stop_platform(void *unused) {
  (void)unused;
  hr_cards_reset();
  hr_audio_game(false);
  hr_preview_reset();
  hr_profile_abort();
  hr_gallery_abort();
  if (!hr_audio_pause())
    return false;
  hr_ble_pause();
  return hr_radio_pause();
}
static void resume_platform(void *unused) {
  (void)unused;
  hr_radio_resume();
  hr_ble_resume();
  view.audio_ok = hr_audio_init();
  hr_audio_volume(view.volume);
}
void app_main(void) {
  trace_boot();
  trace_report();
  ESP_LOGI("power", "boot reset_reason=%d wakeup_cause=%d",
           (int)esp_reset_reason(), (int)esp_sleep_get_wakeup_cause());
  ESP_LOGI("hideseek",
           "v%s / 6 players / ESP-NOW channel 6 / MAC-addressed peers / automatic coordinator",
           esp_app_get_description()->version);
  ESP_LOGI("hideseek",
           "no writes to cardid or original NVS; demo_cfg at 0x360000");
  uint8_t id = 0, volume_value = 90, mac[6] = {0};
  bool saved = hr_storage_load(&id, &volume_value);
  if (saved && !hr_storage_apply_volume_default(id, &volume_value))
    ESP_LOGW("storage", "default volume migration failed; retained old setting");
  ESP_LOGI("audio", "saved playback volume=%u", volume_value);
  view.activation_required = !hr_storage_activated();
  ESP_LOGI("activation", "persisted_enabled=%u", !view.activation_required);
  view.volume = volume_value;
  view.setup = !saved;
  view.choice = saved ? id : 1;
  ESP_ERROR_CHECK(bsp_display_init());
  if (!bsp_lvgl_init())
    return;
  hr_power_init(now_ms(), stop_platform, resume_platform, NULL);
  view.buttons_ok =
      bsp_button_init(NULL, NULL) == ESP_OK && bsp_button_read_mv() >= 0;
  view.radio_ok = hr_radio_init(mac);
  view.audio_ok = hr_audio_init();
  hr_audio_volume(view.volume);
  hr_init(&view.core, id, mac, esp_random(), now_ms());
  hr_identity_enable(&view.core);
  hr_room_enable(&view.core, now_ms());
  view.core.best_valid=hr_storage_best_load(id,&view.core.best_score);
  view.selection = 0;
  hr_view_home(&view, view.now);
  if (id)
    hr_storage_roster_load(&view.core);
  hr_ble_init(id);
  hr_identity_set_sn(&view.core,hr_ble_sn());
  hr_profile_remote_reset(id);
  hr_friend_store_bind(&view.core);
  if (id && !hr_cards_init(id))
    ESP_LOGE("cards", "friend store unavailable");
  if (!hr_gallery_init())
    ESP_LOGE("gallery", "gallery store unavailable");
  hr_intro_t intro;
  hr_intro_init(&intro, hr_storage_welcomed(), now_ms());
  ESP_LOGI("intro", "boot; welcome_seen=%u", intro.welcomed);
  hr_touch_t touch = {0};
  hr_input_t input;
  hr_input_init(&input, now_ms());
  uint32_t last_draw = 0, last_health = 0, save_at = 0;
  bool save_pending = false, configured_welcome=false;
  uint32_t sample_at = now_ms(), max_gap = 0, max_render = 0, max_work = 0;
  unsigned last_flow = ~0u;
  uint32_t best_retry_at=0, roster_retry_at=0, last_game_report=0, last_mesh_report=0;
  bool first_frame = false, audio_paused = false;
  uint32_t audio_retry_at = 0;
  // Control polling must preempt LVGL (priority 4) and wake every 5 ms.
  // Keep the explicit delay below: Wi-Fi/BLE and audio still need CPU time.
  vTaskPrioritySet(NULL, 6);
  for (;;) {
    uint32_t now = now_ms();
    uint32_t gap = now - sample_at;
    sample_at = now;
    if (gap > max_gap)
      max_gap = gap;
    view.now = now;
    view.phone_connected = hr_ble_connected() || hr_management_busy();
    unsigned intro_event = hr_intro_tick(&intro, now, hr_audio_welcome_done());
    if (intro_event & HR_INTRO_START_AUDIO) {
      hr_audio_welcome_start();
      ESP_LOGI("intro", "first welcome started");
    }
    if (intro_event & HR_INTRO_SAVE_WELCOME)
      ESP_LOGI("intro", "welcome flag saved=%u", hr_storage_welcome_save());
    view.intro_kind = intro.kind;
    view.intro_elapsed = now-intro.at;
    uint32_t goodbye_elapsed;
    if (!intro.kind && hr_power_goodbye(now, &goodbye_elapsed)) {
      view.intro_kind = HR_INTRO_SLEEP;
      view.intro_elapsed = goodbye_elapsed;
    }
    view.core.hardware_ok = view.radio_ok && view.audio_ok && view.buttons_ok &&
                            !view.setup && !view.activation_required && !view.phone_connected && !view.intro_kind &&
                            hr_audio_errors() < 3;
    int mv = bsp_button_read_mv();
    int previous_key = input.stable;
    int action = HR_KEY_NONE;
    if (!view.intro_kind && hr_power_filter_input(mv, now))
      action = hr_input_tick(&input, mv, now);
    else
      hr_input_init(&input, now);
    view.up_pressed = input.armed && input.stable == 0;
    if (previous_key != input.stable || action != HR_KEY_NONE)
      ESP_LOGI("keys", "mv=%d stable=%d action=%d page=%u", mv, input.stable,
               action, view.page);
    int effect = hr_handle_key(&view, action, now);
    if (effect == HR_EFFECT_ACTIVATE) {
      if (hr_storage_activation_save()) {
        view.activation_required = false;
        view.save_failed = false;
        hr_input_init(&input, now);
        view.up_pressed = false;
        ESP_LOGI("activation", "enabled by down key and persisted");
      } else {
        view.save_failed = true;
        ESP_LOGW("activation", "save failed; remains inactive");
      }
    }
    if (effect == HR_EFFECT_SAVE_ID) {
      if (hr_storage_save(view.choice, view.volume)) {
        hr_init(&view.core, view.choice, mac, esp_random(), now);
        hr_identity_enable(&view.core);
        hr_identity_set_sn(&view.core,hr_ble_sn());
        hr_room_enable(&view.core, now);
        hr_storage_roster_load(&view.core);
        hr_friend_store_bind(&view.core);
        hr_profile_store_select(view.choice);
        hr_profile_remote_reset(view.choice);
        hr_cards_init(view.choice);
        view.setup = false;
        view.selection = 0;
        hr_view_home(&view, view.now);

        view.core.best_valid=hr_storage_best_load(view.choice,&view.core.best_score);
        view.best_dirty=false;
        view.save_failed = false;
        save_pending = false;
      } else
        view.save_failed = true;
    }
    if (effect == HR_EFFECT_VOLUME_CHANGED) {
      hr_audio_volume(view.volume);
      save_pending = true;
      save_at = now + 1500;
    }
    // Re-evaluate after navigation so the setup screen cannot own a microphone.
    view.core.hardware_ok = view.radio_ok && view.audio_ok && view.buttons_ok &&
                            !view.setup && !view.activation_required && !view.phone_connected && !view.intro_kind &&
                            hr_audio_errors() < 3;
    if(view.page==HR_PAGE_GAME_PLAY) {
      hr_game_tick(&view.game,now,hr_audio_pitch());
      hr_handle_key(&view,HR_KEY_NONE,now);
      if(view.phone_connected && view.page==HR_PAGE_GAME_PLAY)view.page=HR_PAGE_GAME_MENU;
    }
    bool game_audio=view.page==HR_PAGE_GAME_PLAY && view.core.hardware_ok && !hr_ble_upload_busy();
    hr_audio_game(game_audio);
    hr_audio_game_sounds(&view.game,game_audio,now);
    if(view.page==HR_PAGE_GAME_PLAY && hr_audio_mic_stack() && (uint32_t)(now-last_game_report)>=1000) {
      ESP_LOGI("game","phase=%u hz=%u base=%u height=%d progress=%u score=%lu coins=%u rescues=%u mic_stack=%u rms=%u raw=%u",
               view.game.phase,view.game.pitch_hz,view.game.base_hz,view.game.height,view.game.progress,
               (unsigned long)view.game.score,view.game.coins_taken,view.game.rescues,hr_audio_mic_stack(),hr_audio_game_rms(),hr_audio_game_raw_pitch());
      last_game_report=now;
    }
    if(view.best_dirty && !view.phone_connected && !hr_ble_upload_busy() &&
       !view.core.speaker && (int32_t)(now-best_retry_at)>=0) {
      if(hr_storage_best_save(view.core.id,view.core.best_score))view.best_dirty=false;
      best_retry_at=now+2000;
    }
    hr_radio_poll(&view.core, now);
    now = now_ms();
    view.now = now;
    if (save_pending && !view.core.speaker && !view.core.held && !view.setup &&
        (int32_t)(now - save_at) >= 0) {
      view.save_failed = !hr_storage_save(view.core.id, view.volume);
      save_pending = false;
    }
    // A phone edit is already exclusive with PTT. Release idle audio DMA,
    // queues and worker stacks to fit JPEG decode on a board without PSRAM.
    if (view.phone_connected && !audio_paused) {
      audio_paused = hr_audio_pause();
      hr_cards_reset();
      ESP_LOGI("memory", "phone edit audio_paused=%u heap=%u largest=%u",
               audio_paused, (unsigned)esp_get_free_heap_size(),
               (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    } else if (!view.phone_connected && audio_paused &&
               (int32_t)(now - audio_retry_at) >= 0) {
      view.audio_ok = hr_audio_init();
      hr_audio_volume(view.volume);
      audio_paused = !view.audio_ok;
      audio_retry_at = now + 2000;
    }
    hr_ble_poll(&view);
    if(hr_ble_take_reset()) {
      save_pending=false;best_retry_at=0;memset(&touch,0,sizeof touch);hr_input_init(&input,now);
      intro=(hr_intro_t){.welcomed=false,.at=now};configured_welcome=true;
    }
    if(configured_welcome&&!view.phone_connected&&!hr_management_busy()&&!audio_paused) {
      intro=(hr_intro_t){.kind=HR_INTRO_WELCOME,.welcomed=false,.at=now};
      hr_audio_welcome_start();configured_welcome=false;
    }
    hr_friend_store_bind(&view.core);
    if(view.core.roster_dirty&&(int32_t)(now-roster_retry_at)>=0&&!view.core.held&&!view.core.speaker&&!view.phone_connected&&!hr_ble_upload_busy()) {
      if(hr_storage_roster_save(&view.core,view.core.social.friends))view.core.roster_dirty=false;
      else {roster_retry_at=now+2000;ESP_LOGW("storage","peer roster commit failed; will retry");}
    }
    bool touch_opened = hr_touch_tick(&touch, &view, now,
        mv >= 1900 && input.armed && input.stable == -1 && action == HR_KEY_NONE);
    if (touch_opened) {
      // A navigation key must never confirm a newly discovered person.
      hr_input_init(&input, now);
      view.up_pressed = false;
      ESP_LOGI("touch", "%s popup peer=%u rssi=%d; waiting for both confirmations",
               view.core.social.manual ? "manual" : "home",
               view.core.social.candidate,
               view.core.peers[view.core.social.candidate].signal_q8/256);
    }
    unsigned online_mask = 0;
    for (unsigned i = 1; i <= HR_PLAYERS; i++)
      if (i != view.core.id && hr_online(&view.core, i, now) &&
          !hr_conflict(&view.core, i, now))
        online_mask |= 1u << i;
    if((uint32_t)(now-last_mesh_report)>=5000) {
      last_mesh_report=now;unsigned room_mask=0;
      for(unsigned i=1;i<=HR_PLAYERS;i++)if(i!=view.core.id&&hr_room_member(&view.core,i,now))room_mask|=1u<<i;
      ESP_LOGI("mesh","role=%u online=%02x room=%02x host=%u joined=%u ready=%u phone=%u conflict=%u",
        view.core.id,online_mask,room_mask,view.core.host,view.core.room_joined,view.core.hardware_ok,
        view.phone_connected,hr_conflict(&view.core,view.core.id,now));
      for(unsigned i=1;i<=HR_PLAYERS;i++)if(i!=view.core.id&&view.core.peers[i].seen) {
        const hr_peer_t *p=&view.core.peers[i];
        ESP_LOGI("mesh_peer","slot=%u mac=%02x:%02x:%02x:%02x:%02x:%02x member=%u hello_age=%lu rssi=%d raw=%d,%d,%d conflict=%u",
          i,p->mac[0],p->mac[1],p->mac[2],p->mac[3],p->mac[4],p->mac[5],hr_room_member(&view.core,i,now),
          (unsigned long)(now-p->hello_at),p->signal_q8/256,p->signal_window[0],p->signal_window[1],p->signal_window[2],hr_conflict(&view.core,i,now));
      }
      ESP_LOGI("touch_match","best=%u rssi=%d second=%u rssi2=%d reason=%u intent=%u reciprocal=%u stable_ms=%lu",
        touch.nearest,touch.nearest?view.core.peers[touch.nearest].signal_q8/256:-128,
        touch.runner,touch.runner?view.core.peers[touch.runner].signal_q8/256:-128,touch.reason,view.core.touch_target,
        touch.nearest?view.core.peers[touch.nearest].touch_target:0,
        (unsigned long)(touch.mutual_tracking?now-touch.mutual_since:0));
    }
    unsigned flow = view.core.held | (view.core.blocked << 1) |
                    (view.core.speaker << 2) | (view.selection << 5) |
                    (view.page << 8) | (view.phone_connected << 12) |
                    (online_mask << 13) | (hr_can_talk(&view.core, now) << 20) |
                    (view.volume_mode << 21) | (view.setup << 22);
    if (flow != last_flow || action != HR_KEY_NONE) {
      ESP_LOGI("flow",
               "page=%u target=%u online=%02x phone=%u held=%u blocked=%u "
               "speaker=%u tx=%u key=%d volume_page=%u setup=%u ready=%u",
               view.page, view.selection, online_mask, view.phone_connected,
               view.core.held, view.core.blocked, view.core.speaker,
               hr_can_talk(&view.core, now), action, view.volume_mode,
               view.setup, view.core.hardware_ok);
      last_flow = flow;
      if(view.core.held || view.core.speaker)
        ESP_LOGI("floor","host=%u request=%lu grant_request=%lu token=%lu grant_age=%lu echo_age=%lu press_age=%lu",
          view.core.host,(unsigned long)view.core.request,(unsigned long)view.core.floor_request,
          (unsigned long)view.core.token,(unsigned long)(now-view.core.floor_at),
          (unsigned long)(now-view.core.floor_client_at),(unsigned long)(now-view.core.press_at));
    }
    view.nearest = hr_nearest(&view.core, now, view.nearest);
    hr_social_tick(&view.core, now);
    if (!view.phone_connected && !hr_ble_upload_busy())
      hr_cards_tick(&view.core, now);
    hr_preview_tick(&view.core, view.page==HR_PAGE_PAIR &&
        !view.activation_required && !view.phone_connected && !view.intro_kind &&
        !hr_ble_upload_busy(), now);
    const void *next_preview = hr_preview_image();
    if (next_preview && !view.pair_preview)
      ESP_LOGI("touch", "preview ready peer=%u before_confirm=%u",
               view.core.social.candidate, !view.core.social.accepted);
    view.pair_preview = next_preview;
    view.card_progress = hr_cards_progress(&view.core);
    if(!view.phone_connected&&!hr_ble_upload_busy())
      hr_cards_commit(&view.core,now_ms(),hr_storage_roster_save);
    view.receiving = hr_audio_recent(now);
    view.audio_ok = view.audio_ok && hr_audio_errors() < 3;
    view.battery_percent = hr_power_battery();
    if ((uint32_t)(now - last_draw) >= (view.page==HR_PAGE_GAME_PLAY ? 40u : 100u) && bsp_lvgl_lock(5)) {
      uint32_t render_at = now_ms();
      hr_ui_render(&view);
      if (!first_frame) {
        hr_power_first_frame(now);
        first_frame = true;
      }
      uint32_t render_time = now_ms() - render_at;
      if (render_time > max_render)
        max_render = render_time;
      bsp_lvgl_unlock();
      last_draw = now;
    }
    uint32_t work_time = now_ms() - sample_at;
    if (work_time > max_work)
      max_work = work_time;
    if ((uint32_t)(now - last_health) >= 10000) {
      unsigned captured, played, peak;
      hr_audio_stats(&captured, &played, &peak);
      ESP_LOGI("audio", "capture=%u playback=%u peak=%u held=%u blocked=%u",
               captured, played, peak, view.core.held, view.core.blocked);
      ESP_LOGI(
          "health",
          "id=%u heap=%u min=%u dma=%u dma_max=%u drops=%u audio_err=%u speaker=%u main_stack=%u",
          view.core.id, (unsigned)esp_get_free_heap_size(),
          (unsigned)esp_get_minimum_free_heap_size(),
          (unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),
          (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA),hr_radio_drops(),
          hr_audio_errors(), view.core.speaker,
          (unsigned)uxTaskGetStackHighWaterMark(NULL));
      ESP_LOGI("keys",
               "max_poll_gap_ms=%lu render_ms=%lu work_ms=%lu priority=%u",
               (unsigned long)max_gap, (unsigned long)max_render,
               (unsigned long)max_work, (unsigned)uxTaskPriorityGet(NULL));
      hr_radio_diagnostics();
      ESP_LOGI("battery", "display_soc=%d", hr_power_battery());
      bsp_battery_report();
      hr_cards_stats_t card_stats;hr_cards_stats(&card_stats,now);
      ESP_LOGI("cards_detail","reason=%u rx=%u/%u tx_ack=%u/%u received=%u acked=%u commits=%u elapsed=%lu",
        card_stats.reason,card_stats.rx_bytes,card_stats.rx_total,card_stats.tx_confirmed,
        card_stats.tx_total,card_stats.received,card_stats.acknowledged,card_stats.commit_attempts,
        (unsigned long)card_stats.elapsed);
      unsigned candidate=view.core.social.candidate;
      ESP_LOGI("cards", "peer=%u progress=%u accepted=%u pending=%u saved=%u complete=%u failed=%u peer_flags=%u session=%lu peer_session=%lu",
        candidate,hr_cards_progress(&view.core),view.core.social.accepted,view.core.social.pending,
        view.core.social.saved,view.core.social.complete,view.core.social.failed,
        candidate<=HR_PLAYERS?view.core.peers[candidate].social_flags:0,
        (unsigned long)view.core.social.session,(unsigned long)view.core.social.peer_session);
      if(candidate&&candidate<=HR_PLAYERS) {
        const hr_peer_t *cp=&view.core.peers[candidate];
        ESP_LOGI("cards_session","own_boot=%lu peer_boot=%lu accepted_boot=%lu own_session=%lu accepted_session=%lu peer_session=%lu peer_boot_token=%lu peer_session_token=%lu target=%u",
          (unsigned long)view.core.boot,(unsigned long)cp->boot,(unsigned long)view.core.social.peer_boot,
          (unsigned long)view.core.social.session,(unsigned long)view.core.social.peer_session,
          (unsigned long)cp->social_session,(unsigned long)cp->social_boot,(unsigned long)cp->social_token,cp->social_target);
      }
      trace_report();
      max_gap = max_render = max_work = 0;
      last_health = now;
    }
    hr_power_tick(
        now_ms(), action != HR_KEY_NONE || view.receiving || touch_opened,
        mv < 1900 || view.core.held || view.core.speaker ||
            view.phone_connected || hr_ble_upload_busy() ||
            (view.core.social.pending && !view.core.social.complete) ||
            save_pending || view.core.roster_dirty || view.best_dirty || view.page==HR_PAGE_GAME_PLAY || intro.kind || view.room_joining);
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}
