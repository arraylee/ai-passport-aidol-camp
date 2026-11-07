#include "hr_ble.h"
#include "hr_management.h"
#include "bsp_display.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/util/util.h"
#include "hr_cards.h"
#include "hr_edit.h"
#include "hr_gallery.h"
#include "hr_platform.h"
#include "hr_profile_store.h"
#include "nimble/nimble_port.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#define UUID(N)                                                                \
  BLE_UUID128_INIT((N), 0, 0, 0, 0, 0, 0, 0, 0x44, 0x52, 0x41, 0x43, 0x45,     \
                   0x41, 0x52, 0x54)
static const ble_uuid128_t service = UUID(0), command = UUID(0x10),
                           notice = UUID(0x11), identity = UUID(0x12),
                           score = UUID(0x13), management=UUID(0x20);
typedef struct {
  uint32_t generation;
  uint16_t length;
  uint8_t type, data[256];
} job_t;
typedef struct {
  uint32_t generation;
  uint8_t type, status;
} reply_t;
static QueueHandle_t jobs, replies;
static atomic_uint generation;
static atomic_bool connected, host_ready;
static bool reset_applied;
bool hr_ble_take_reset(void){bool r=reset_applied;reset_applied=false;return r;}
static uint32_t processing;
static bool image_fullscreen;
bool hr_ble_upload_busy(void) {
  return hr_profile_upload_busy() || hr_gallery_busy() || hr_management_busy();
}
static uint16_t connection = BLE_HS_CONN_HANDLE_NONE, notify_handle;
static uint8_t address_type, public_identity[72];
static char device_sn[25];
static hr_frame_t frame;
static struct ble_npl_event reply_event;
static struct ble_npl_callout timeout;
static uint32_t ms(void) { return esp_timer_get_time() / 1000; }
bool hr_ble_connected(void) { return atomic_load(&connected); }
bool hr_profile_session_valid(void) {
  return hr_ble_connected() && processing == atomic_load(&generation);
}
static void notify(unsigned type, unsigned status) {
  if (connection == BLE_HS_CONN_HANDLE_NONE)
    return;
  uint8_t p[] = {type, status};
  struct os_mbuf *m = ble_hs_mbuf_from_flat(p, 2);
  if (m)
    ble_gatts_notify_custom(connection, notify_handle, m);
}
static void drain(struct ble_npl_event *e) {
  (void)e;
  reply_t r;
  while (xQueueReceive(replies, &r, 0) == pdTRUE)
    if (r.generation == atomic_load(&generation) && hr_ble_connected())
      notify(r.type, r.status);
}
static void expired(struct ble_npl_event *e) {
  (void)e;
  if (frame.used) {
    unsigned type = frame.used > 1 ? frame.bytes[1] : 0;
    memset(&frame, 0, sizeof frame);
    notify(type, 0x11);
  }
}
static int access(uint16_t conn, uint16_t handle,
                  struct ble_gatt_access_ctxt *ctx, void *arg) {
  (void)conn;
  (void)handle;
  if (ctx->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    const uint8_t empty[9] = {1};
    return os_mbuf_append(ctx->om, arg ? empty : public_identity,
                          arg ? sizeof empty : sizeof public_identity)
               ? BLE_ATT_ERR_INSUFFICIENT_RES
               : 0;
  }
  if (ctx->op != BLE_GATT_ACCESS_OP_WRITE_CHR)
    return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
  uint8_t buf[260];
  uint16_t n = 0;
  if (ble_hs_mbuf_to_flat(ctx->om, buf, sizeof buf, &n)) {
    memset(&frame, 0, sizeof frame);
    notify(0, 0x15);
    return 0;
  }
  unsigned s = hr_frame_push(&frame, buf, n, ms());
  if (s == 1) {
    job_t j = {.generation = atomic_load(&generation),
               .type = frame.bytes[1],
               .length = frame.wanted - 4};
    memcpy(j.data, frame.bytes + 4, j.length);
    if (xQueueSend(jobs, &j, pdMS_TO_TICKS(500)) != pdTRUE)
      notify(j.type, 0x1b);
    memset(&frame, 0, sizeof frame);
    ble_npl_callout_stop(&timeout);
  } else if (s)
    notify(0, s);
  else
    ble_npl_callout_reset(&timeout, ble_npl_time_ms_to_ticks32(2100));
  return 0;
}
static int management_access(uint16_t conn,uint16_t handle,struct ble_gatt_access_ctxt*ctx,void*arg){
 (void)conn;(void)handle;(void)arg;uint8_t b[HR_MANAGEMENT_BYTES];hr_management_read(b);
 return os_mbuf_append(ctx->om,b,sizeof b)?BLE_ATT_ERR_INSUFFICIENT_RES:0;
}
static const struct ble_gatt_chr_def chars[] = {
 {.uuid=&management.u,.access_cb=management_access,.flags=BLE_GATT_CHR_F_READ},
    {.uuid = &command.u,
     .access_cb = access,
     .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP},
    {.uuid = &notice.u,
     .access_cb = access,
     .flags = BLE_GATT_CHR_F_NOTIFY,
     .val_handle = &notify_handle},
    {.uuid = &identity.u, .access_cb = access, .flags = BLE_GATT_CHR_F_READ},
    {.uuid = &score.u,
     .access_cb = access,
     .arg = (void *)1,
     .flags = BLE_GATT_CHR_F_READ},
    {0}};
static const struct ble_gatt_svc_def services[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &service.u,
     .characteristics = chars},
    {0}};
static int gap(struct ble_gap_event *, void *);
static void advertise(void) {
  struct ble_hs_adv_fields fields = {.flags = BLE_HS_ADV_F_DISC_GEN |
                                              BLE_HS_ADV_F_BREDR_UNSUP,
                                     .name = (uint8_t *)device_sn,
                                     .name_len = strlen(device_sn),
                                     .name_is_complete = 1};
  struct ble_hs_adv_fields scan = {.uuids128 = (ble_uuid128_t *)&service,
                                   .num_uuids128 = 1,
                                   .uuids128_is_complete = 1};
  struct ble_gap_adv_params params = {.conn_mode = BLE_GAP_CONN_MODE_UND,
                                      .disc_mode = BLE_GAP_DISC_MODE_GEN,
                                      .itvl_min = 320,
                                      .itvl_max = 480};
  int rc = ble_gap_adv_set_fields(&fields);
  if (!rc)
    rc = ble_gap_adv_rsp_set_fields(&scan);
  if (!rc)
    rc = ble_gap_adv_start(address_type, NULL, BLE_HS_FOREVER, &params, gap,
                           NULL);
  if (rc)
    ESP_LOGW("miniapp", "advertise rc=%d", rc);
}
static int gap(struct ble_gap_event *e, void *arg) {
  (void)arg;
  if (e->type == BLE_GAP_EVENT_CONNECT || e->type == BLE_GAP_EVENT_DISCONNECT) {
    bool ok = e->type == BLE_GAP_EVENT_CONNECT && !e->connect.status;
    connection = ok ? e->connect.conn_handle : BLE_HS_CONN_HANDLE_NONE;
    atomic_store(&connected, ok);
    hr_management_connection(ok);
    atomic_fetch_add(&generation, 1);
    memset(&frame, 0, sizeof frame);
    ble_npl_callout_stop(&timeout);
    ESP_LOGI("miniapp", "connected=%u", ok);
    if (!ok)
      advertise();
  }
  return 0;
}
static void sync_host(void) {
  if (!ble_hs_util_ensure_addr(0) && !ble_hs_id_infer_auto(0, &address_type))
    atomic_store(&host_ready,true);
}
static void host(void *arg) {
  (void)arg;
  nimble_port_run();
  vTaskDelete(NULL);
}
static void read_identity(void) {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(device_sn, sizeof device_sn, "%02x%02x%02x%02x%02x%02x", mac[0],
           mac[1], mac[2], mac[3], mac[4], mac[5]);
  char hw[9] = {0};
  bool provisioned = false;
  // Read-only initialization; never erase, repair, or write original identity.
  if (nvs_flash_init_partition("cardid") == ESP_OK) {
    nvs_handle_t h;
    if (nvs_open_from_partition("cardid", "folotoy-key", NVS_READONLY, &h) ==
        ESP_OK) {
      size_t n = sizeof device_sn;
      if (nvs_get_str(h, "DeviceKey", device_sn, &n) != ESP_OK || !device_sn[0])
        snprintf(device_sn, sizeof device_sn, "%02x%02x%02x%02x%02x%02x",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
      n = sizeof hw;
      nvs_get_str(h, "HardwareVersion", hw, &n);
      size_t secret = 0, product = 0;
      provisioned = nvs_get_str(h, "DeviceSecret", NULL, &secret) == ESP_OK &&
                    secret > 1 &&
                    nvs_get_str(h, "ProductKey", NULL, &product) == ESP_OK &&
                    product > 1;
      nvs_close(h);
    }
  }
  // Legacy miniapp parser requires version 1; key slot remains entirely zero.
  public_identity[0] = 1;
  public_identity[1] = 2 | (provisioned ? 1 : 0);
  memcpy(public_identity + 2, device_sn, strlen(device_sn));
  memcpy(public_identity + 58, hw, strnlen(hw, 8));
}
const char *hr_ble_sn(void){return device_sn;}
bool hr_ble_init(unsigned id) {
  read_identity();
  bool store = hr_profile_store_init(id);
  if (!store) {
    ESP_LOGE("miniapp", "profile storage unavailable");
    return false;
  }
  hr_management_init(device_sn);
  jobs = xQueueCreate(8, sizeof(job_t));
  replies = xQueueCreate(16, sizeof(reply_t));
  if (!jobs || !replies || nimble_port_init() != ESP_OK)
    return false;
  ble_npl_event_init(&reply_event, drain, NULL);
  ble_npl_callout_init(&timeout, nimble_port_get_dflt_eventq(), expired, NULL);
  ble_att_set_preferred_mtu(256);
  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_svc_gap_device_name_set(device_sn);
  ble_hs_cfg.sync_cb = sync_host;
  ble_hs_cfg.sm_bonding = 0;
  ble_hs_cfg.sm_mitm = 0;
  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  if (ble_gatts_count_cfg(services) || ble_gatts_add_svcs(services))
    return false;
  if (xTaskCreate(host, "miniapp", 4096, NULL, configMAX_PRIORITIES - 4,
                  NULL) != pdPASS)
    return false;
  ESP_LOGI("miniapp", "TRAECARD ready; original SN; game id=%u", id);
  return true;
}
void hr_ble_poll(hr_view_t *v) {
  static uint32_t seen;
  uint32_t gen = atomic_load(&generation);
  if (gen != seen) {
    hr_profile_abort();
    hr_gallery_abort();
    hr_cards_reset();
    image_fullscreen = false;
    seen = gen;
  }
  hr_profile_expire(ms());
  hr_gallery_expire(ms());
  v->phone_connected = hr_ble_connected() || hr_management_busy();
  if(hr_management_poll(v))reset_applied=true;
  if(atomic_load(&host_ready)&&!hr_ble_connected()&&!ble_gap_adv_active())advertise();
  job_t j;
  if (!jobs || xQueueReceive(jobs, &j, 0) != pdTRUE)
    return;
  if (!v->phone_connected || j.generation != gen)
    return;
  processing = gen;
  if (!bsp_lvgl_lock(100)) {
    reply_t r = {gen, j.type, 0x1b};
    xQueueSend(replies, &r, 0);
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &reply_event);
    return;
  }
  unsigned status = 0x18;
  if(j.type==0x30)status=hr_management_command(v,j.data,j.length);
  else if(hr_management_busy())status=0x1b;
  else if (j.type == 1) {
    if (hr_ble_upload_busy())
      status = 0x1b;
    else {
      status = hr_profile_config(j.data, j.length);
      if (status == 1) {
        cJSON *config = cJSON_ParseWithLength((const char *)j.data, j.length);
        const cJSON *mode =
            cJSON_GetObjectItemCaseSensitive(config, "img_mode");
        if (cJSON_IsString(mode))
          image_fullscreen = !strcmp(mode->valuestring, "fullscreen");
        cJSON_Delete(config);
      }
    }
  } else if (j.type == 2 && !hr_audio_paused()) {
    status = 0x1b;
  } else if (j.type == 2) {
    if (j.length && j.data[0] == 0)
      hr_cards_reset();
    status = image_fullscreen ? hr_gallery_image(j.data, j.length, ms())
                              : hr_profile_image(j.data, j.length, ms());
  }
  hr_ui_render(v);
  bsp_lvgl_unlock();
  reply_t r = {gen, j.type, status};
  xQueueSend(replies, &r, 0);
  ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &reply_event);
  if (status)
    ESP_LOGI("miniapp", "type=%u result=0x%02x", j.type, status);
}

void hr_ble_pause(void) { ble_gap_adv_stop(); }
void hr_ble_resume(void) {
  if (!hr_ble_connected())
    advertise();
}
