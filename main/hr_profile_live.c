#include "hr_people.h"
#include "hr_profiles.h"
#include <stdio.h>
#include <string.h>
static hr_profile_t live[7];
static bool remote_avatar[7];
static char titles[7][49];
void hr_profile_set_title(unsigned id, const char *title) {
  if (id >= 1 && id <= 6)
    snprintf(titles[id], 49, "%s", title);
}
const char *hr_display_title(unsigned id) {
  return id >= 1 && id <= 6 ? titles[id] : "";
}
static char edited_names[7][49], edited_intros[7][85];
const hr_profile_t *hr_profile_base(unsigned id);
const hr_profile_t *hr_profile(unsigned id) {
  if (id < 1 || id > 6)
    id = 1;
  return live[id].name ? &live[id] : hr_profile_base(id);
}
void hr_profile_override(unsigned id, const char *name, const char *intro,
                         const lv_image_dsc_t *card,
                         const lv_image_dsc_t *small) {
  if (id < 1 || id > 6)
    return;
  live[id] = *hr_profile_base(id);
  snprintf(edited_names[id], 49, "%s", name);
  snprintf(edited_intros[id], 85, "%s", intro);
  live[id].name = edited_names[id];
  if (strcmp(name, hr_profile_base(id)->name))
    live[id].url = "";
  live[id].signature = edited_intros[id];
  if (card)
    live[id].card = card;
  if (small)
    live[id].small = small;
}
void hr_profile_clear(unsigned id) {
  if (id >= 1 && id <= 6) {
    memset(&live[id], 0, sizeof live[id]);
    titles[id][0] = 0;
    remote_avatar[id] = false;
  }
}
const char *hr_display_name(unsigned id) {
  return id >= 1 && id <= 6 ? hr_profile(id)->name : hr_name(0);
}
bool hr_profile_remote_avatar(unsigned id) {
  return id >= 1 && id <= 6 && remote_avatar[id];
}
void hr_profile_remote(unsigned id, const char *name, const char *intro,
                       bool avatar) {
  if (id < 1 || id > 6)
    return;
  remote_avatar[id] = avatar;
  hr_profile_override(id, name, intro, NULL, NULL);
  /* A local slot is not a persona: never borrow the seed owner avatar. */
  live[id].card=live[id].small=live[id].portrait=live[id].gallery=NULL;
}

void hr_profile_set_portrait(unsigned id, const lv_image_dsc_t *portrait) {
  if (id >= 1 && id <= 6)
    live[id].portrait = portrait;
}

void hr_profile_remote_reset(unsigned self) {
  for(unsigned id=1;id<=6;id++)if(id!=self) {
    hr_profile_clear(id);
    hr_profile_remote(id,"设备","",false);
  }
}
