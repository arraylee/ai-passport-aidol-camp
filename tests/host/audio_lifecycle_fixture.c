#include "hr_core.h"
#include "hr_sfx.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#define ESP_OK 0
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_LOGE(...) ((void)0)
typedef unsigned TickType_t;
typedef void (*TaskFunction_t)(void *);
typedef struct { unsigned id; bool live; } fake_queue_t;
typedef struct { unsigned bit; bool live; } fake_task_t;
typedef fake_queue_t *QueueHandle_t;
typedef fake_task_t *TaskHandle_t;
typedef struct {
  uint8_t data[HR_AUDIO];
  uint32_t gate, at;
} audio_t;

/* Production-owned state, matching the dependencies of the extracted bodies. */
static QueueHandle_t tx_audio, rx_audio;
static TaskHandle_t capture_handle, playback_handle;
static atomic_bool audio_stop;
static hr_sfx_bus_t game_sound;
static atomic_uint audio_ack, tx_gate, rx_gate, welcome_request;
static bool audio_ready;

/* Target-only resources and deterministic worker acknowledgement scheduling. */
static fake_queue_t queue_slots[32];
static fake_task_t task_slots[32];
static unsigned queue_calls, task_calls, queue_live, task_live;
static unsigned queue_deletes, task_deletes, bsp_inits, bsp_deinits;
static unsigned fail_queues, fail_task_call;
static bool fail_bsp_init, fail_format, bsp_live;
static unsigned ack_enabled, mic_ack_after, speaker_ack_after;
static uint32_t fake_now, delayed;
static unsigned ack_at_delete, deinit_at, premature_releases;
static unsigned capture_creations, playback_creations;

static unsigned ms(void) { return fake_now; }
static void capture_task(void *unused) { (void)unused; }
static void playback_task(void *unused) { (void)unused; }

static int bsp_audio_init(void) {
  assert(!bsp_live && task_live == 0 && queue_live == 0);
  bsp_inits++;
  /* Initialization may allocate some peripherals before reporting failure. */
  bsp_live = true;
  return fail_bsp_init ? -1 : ESP_OK;
}
static int bsp_audio_set_format(unsigned hz, unsigned bits, unsigned channels) {
  assert(bsp_live && hz == 16000 && bits == 16 && channels == 1);
  return fail_format ? -1 : ESP_OK;
}
static void bsp_audio_deinit(void) {
  if (task_live)
    premature_releases++;
  assert(task_live == 0); /* DMA/codec must outlive every task using it. */
  bsp_deinits++;
  bsp_live = false;
  deinit_at = delayed;
}
static QueueHandle_t xQueueCreate(unsigned count, size_t item_size) {
  assert(bsp_live && item_size == sizeof(audio_t));
  unsigned call = ++queue_calls;
  assert(call <= 32);
  /* A transmit queue holds four frames; a receive queue holds eight. */
  assert(count == 4 || count == 8);
  if ((fail_queues >> (call - 1)) & 1u)
    return NULL;
  fake_queue_t *queue = &queue_slots[call - 1];
  assert(!queue->live);
  *queue = (fake_queue_t){.id = count, .live = true};
  queue_live++;
  return queue;
}
static void vQueueDelete(QueueHandle_t queue) {
  assert(queue && queue->live && task_live == 0);
  queue->live = false;
  queue_live--;
  queue_deletes++;
}
static int xTaskCreate(TaskFunction_t body, const char *name, unsigned stack,
                       void *arg, unsigned priority, TaskHandle_t *handle) {
  (void)name;
  assert(bsp_live && tx_audio && tx_audio->live && rx_audio && rx_audio->live);
  assert(stack >= 4096 && priority > 0 && arg == NULL && handle);
  assert(body == capture_task || body == playback_task);
  unsigned bit = body == capture_task ? 1 : 2;
  unsigned call = ++task_calls;
  assert(call <= 32);
  if (call == fail_task_call) {
    *handle = NULL;
    return 0;
  }
  /* Resume must never create a second live capture or playback worker. */
  for (unsigned i = 0; i < 32; ++i)
    assert(!task_slots[i].live || task_slots[i].bit != bit);
  fake_task_t *task = &task_slots[call - 1];
  *task = (fake_task_t){.bit = bit, .live = true};
  *handle = task;
  task_live++;
  if (bit == 1) capture_creations++;
  else playback_creations++;
  return pdPASS;
}
static void vTaskDelete(TaskHandle_t task) {
  assert(task && task->live && atomic_load(&audio_stop));
  unsigned live_bits = 0;
  for (unsigned i = 0; i < 32; ++i)
    if (task_slots[i].live) live_bits |= task_slots[i].bit;
  /* Even deleting the first worker must wait for *all* current workers. */
  assert((atomic_load(&audio_ack) & live_bits) == live_bits);
  ack_at_delete |= atomic_load(&audio_ack);
  task->live = false;
  task_live--;
  task_deletes++;
}
static void vTaskDelay(TickType_t ticks) {
  assert(ticks > 0);
  fake_now += ticks;
  delayed += ticks;
  /* The fixture schedules worker ACKs; it does not implement lifecycle logic. */
  if (atomic_load(&audio_stop)) {
    for (unsigned i = 0; i < 32; ++i) {
      fake_task_t *task = &task_slots[i];
      if (!task->live || !(ack_enabled & task->bit)) continue;
      unsigned when = task->bit == 1 ? mic_ack_after : speaker_ack_after;
      if (delayed >= when) atomic_fetch_or(&audio_ack, task->bit);
    }
  }
  assert(delayed < 5000); /* A missing timeout fails instead of hanging. */
}

#include "production_audio_lifecycle.inc"

static void clean(void) {
  assert(!audio_ready && !capture_handle && !playback_handle);
  assert(!tx_audio && !rx_audio && !bsp_live);
  assert(task_live == 0 && queue_live == 0 && premature_releases == 0);
  assert(hr_audio_paused());
}
static void reset(void) {
  clean();
  memset(queue_slots, 0, sizeof queue_slots);
  memset(task_slots, 0, sizeof task_slots);
  queue_calls = task_calls = queue_live = task_live = 0;
  queue_deletes = task_deletes = bsp_inits = bsp_deinits = 0;
  fail_queues = fail_task_call = 0;
  fail_bsp_init = fail_format = false;
  ack_enabled = 3; mic_ack_after = 5; speaker_ack_after = 15;
  fake_now = 1000; delayed = 0;
  ack_at_delete = deinit_at = premature_releases = 0;
  capture_creations = playback_creations = 0;
  atomic_store(&audio_stop, false);
  atomic_store(&audio_ack, 0);
  atomic_store(&tx_gate, 77);
  atomic_store(&rx_gate, 88);
}
static void running(void) {
  assert(audio_ready && !hr_audio_paused() && bsp_live);
  assert(task_live == 2 && queue_live == 2);
  assert(capture_handle && capture_handle->live);
  assert(playback_handle && playback_handle->live);
  assert(!atomic_load(&audio_stop));
}
static void test_queue_failures(void) {
  /* Reject first, second, or both queues; the surviving allocation must go. */
  for (unsigned mask = 1; mask <= 3; ++mask) {
    reset(); fail_queues = mask;
    assert(!hr_audio_init()); clean();
    assert(task_calls == 0 && queue_calls == 2);
    assert(queue_deletes == (mask == 3 ? 0u : 1u));
    assert(bsp_inits == 1 && bsp_deinits == 1);
    fail_queues = 0;
    assert(hr_audio_init()); running();
    assert(hr_audio_pause()); clean();
  }
  puts("PASS queue allocation failures release both/surviving queue and retry");
}
static void test_task_failures(void) {
  for (unsigned failed = 1; failed <= 2; ++failed) {
    reset(); fail_task_call = failed;
    assert(!hr_audio_init()); clean();
    assert(task_calls == failed && task_deletes == failed - 1);
    assert(queue_deletes == 2 && bsp_deinits == 1);
    if (failed == 2) assert(ack_at_delete == 1 && deinit_at >= mic_ack_after);
    fail_task_call = 0;
    assert(hr_audio_init()); running();
    unsigned before = task_calls;
    assert(hr_audio_init()); running(); assert(task_calls == before);
    assert(hr_audio_pause()); clean();
  }
  puts("PASS task creation failure waits for existing worker and permits retry");
}
static void test_both_workers_acknowledge(void) {
  reset(); assert(hr_audio_init()); running();
  hr_sfx_enable(&game_sound,true,ms());hr_sfx_request(&game_sound,HR_SFX_WIN,ms());
  assert(atomic_load(&game_sound.pending)==HR_SFX_WIN);
  assert(hr_audio_pause()); clean();
  assert(!atomic_load(&game_sound.pending)&&!atomic_load(&game_sound.enabled));
  assert(ack_at_delete == 3 && task_deletes == 2);
  assert(deinit_at >= speaker_ack_after && delayed == speaker_ack_after);
  assert(atomic_load(&tx_gate) == 0 && atomic_load(&rx_gate) == 0);
  assert(hr_audio_init()); running();
  assert(capture_creations == 2 && playback_creations == 2);
  assert(hr_audio_pause()); clean();
  puts("PASS both worker acknowledgements precede task/DMA/queue destruction");
}
static void test_timeout_retains_resources(void) {
  /* Neither worker, or either single worker, can authorize destruction. */
  for (unsigned allowed = 0; allowed <= 2; ++allowed) {
    reset(); assert(hr_audio_init()); running();
    ack_enabled = allowed;
    atomic_store(&audio_ack, 3); /* Stale ACKs cannot authorize this new pause. */
    if (allowed == 2) fake_now = UINT32_MAX - 100; /* timeout across rollover */
    assert(!hr_audio_pause()); running();
    assert(bsp_deinits == 0 && task_deletes == 0 && queue_deletes == 0);
    assert(delayed >= 1200 && delayed < 1300);
    assert(!atomic_load(&audio_stop));
    unsigned before = task_calls;
    assert(hr_audio_init()); running(); assert(task_calls == before);
    ack_enabled = 3;
    assert(hr_audio_pause()); clean();
    assert(hr_audio_init()); running();
    assert(capture_creations == 2 && playback_creations == 2);
    assert(hr_audio_pause()); clean();
  }
  puts("PASS timeout preserves every live resource and resume does not duplicate tasks");
}
static void test_partial_start_timeout(void) {
  reset(); fail_task_call = 2; ack_enabled = 0;
  assert(!hr_audio_init());
  assert(!audio_ready && !hr_audio_paused());
  assert(capture_handle && capture_handle->live && !playback_handle);
  assert(task_live == 1 && queue_live == 2 && bsp_live);
  assert(bsp_deinits == 0 && task_deletes == 0 && queue_deletes == 0);
  unsigned before = task_calls, before_bsp = bsp_inits;
  fail_task_call = 0;
  for (unsigned i = 0; i < 3; ++i) assert(!hr_audio_init());
  assert(task_calls == before && bsp_inits == before_bsp);
  ack_enabled = 1;
  assert(hr_audio_pause()); clean();
  assert(hr_audio_init()); running();
  assert(capture_creations == 2 && playback_creations == 1);
  ack_enabled = 3;
  assert(hr_audio_pause()); clean();
  puts("PASS partial-start timeout prevents duplicate workers until acknowledged cleanup");
}
static void test_bsp_failure_and_empty_pause(void) {
  for (unsigned stage = 0; stage < 2; ++stage) {
    reset(); fail_bsp_init = stage == 0; fail_format = stage == 1;
    assert(!hr_audio_init()); clean();
    assert(bsp_inits == 1 && bsp_deinits == 1 && queue_calls == 0 && task_calls == 0);
    fail_bsp_init = fail_format = false;
    assert(hr_audio_init()); running();
    assert(hr_audio_pause()); clean();
    unsigned before = task_deletes;
    assert(hr_audio_pause()); clean(); assert(task_deletes == before);
  }
  puts("PASS partial BSP failure cleans peripherals and repeated pause is safe");
}
int main(void) {
  test_queue_failures();
  test_task_failures();
  test_both_workers_acknowledge();
  test_timeout_retains_resources();
  test_partial_start_timeout();
  test_bsp_failure_and_empty_pause();
  puts("audio lifecycle: production init/pause/paused functions passed all scenarios");
  return 0;
}
