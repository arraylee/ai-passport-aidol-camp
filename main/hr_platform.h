#pragma once
#include "hr_core.h"
#include "hr_game.h"
bool hr_radio_init(uint8_t mac[6]);
void hr_radio_poll(hr_core_t *c, uint32_t now);
bool hr_audio_init(void);
void hr_audio_volume(uint8_t volume);
void hr_audio_gate(hr_core_t *c, uint32_t now, bool enabled);
unsigned hr_radio_drops(void);
unsigned hr_audio_errors(void);
bool hr_audio_recent(uint32_t now);
bool hr_storage_load(uint8_t *id, uint8_t *volume);
bool hr_storage_save(uint8_t id, uint8_t volume);

bool hr_storage_friends_load(uint8_t id, uint8_t *mask);
bool hr_storage_friends_save(uint8_t id, uint8_t mask);
void hr_audio_stats(unsigned *captured, unsigned *played, unsigned *peak);
void hr_radio_diagnostics(void);

bool hr_audio_pause(void);
bool hr_radio_pause(void);
void hr_radio_resume(void);

bool hr_audio_paused(void);

void hr_audio_welcome_start(void);
bool hr_audio_welcome_done(void);
bool hr_storage_welcomed(void);
bool hr_storage_welcome_save(void);

bool hr_storage_activated(void);
bool hr_storage_activation_save(void);

/* Initialize upgraded settings to 90 once, then retain user adjustments. */
bool hr_storage_apply_volume_default(uint8_t id, uint8_t *value);

void hr_audio_game(bool enabled);
unsigned hr_audio_pitch(void);
unsigned hr_audio_mic_stack(void);
unsigned hr_audio_game_rms(void);
unsigned hr_audio_game_raw_pitch(void);
bool hr_storage_best_load(unsigned id,uint32_t *score);
bool hr_storage_best_save(unsigned id,unsigned score);

void hr_audio_game_sounds(const hr_game_t *game,bool enabled,uint32_t now);

bool hr_storage_roster_load(hr_core_t *c);
bool hr_storage_roster_save(const hr_core_t *c,uint8_t friends);

bool hr_storage_config_mark(uint8_t role,uint8_t action,uint32_t txn);
bool hr_storage_config_pending(uint8_t *role,uint8_t *action,uint32_t *txn);
bool hr_storage_config_clear(void);
bool hr_storage_config_reset(uint8_t role);
bool hr_storage_config_erase(unsigned region,uint32_t offset);
