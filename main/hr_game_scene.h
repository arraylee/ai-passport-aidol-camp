#pragma once
#include "lvgl.h"
#include "hr_game.h"
lv_obj_t *hr_game_scene_create(lv_obj_t *parent);
void hr_game_scene_update(lv_obj_t *scene,const hr_game_t *game,uint32_t now);
