#include "hr_touch.h"
#include "hr_ui.h"
/* RSSI is a conservative proximity hint, never a physical-contact sensor. */
static bool available(const hr_core_t *c, unsigned id, uint32_t now) {
  const hr_peer_t *p = &c->peers[id];
  return id != c->id && hr_online(c, id, now) &&
         !hr_conflict(c, id, now) && !hr_conflict(c, c->id, now) &&
         (uint32_t)(now-p->hello_at) < 500 && p->samples &&
         c->social.manual == !!(p->social_flags & HR_SOCIAL_MANUAL_FLAG) &&
         (p->social_flags & 2) && p->social_session &&
         (!(p->social_flags & 4) || p->social_target == c->id);
}
static void proposal(hr_core_t *c, unsigned id, uint32_t now) {
  if(c->touch_target!=id) {c->touch_target=id;c->next_hello=now;}
}
static void reset_match(hr_touch_t *s) {
  s->candidate=0;s->mutual_tracking=false;
}
bool hr_touch_tick(hr_touch_t *s, hr_view_t *v, uint32_t now, bool neutral) {
  hr_core_t *c=&v->core;
  s->reason=HR_TOUCH_DISABLED;s->nearest=s->runner=0;
  for(unsigned id=1;id<=HR_PLAYERS;id++) {
    unsigned bit=1u<<id;
    if(!(s->blocked & bit)) continue;
    bool released=true;
    for(unsigned i=0;i<3;i++)if(c->peers[id].signal_window[i]>=HR_TOUCH_RSSI-4)released=false;
    bool away=!hr_online(c,id,now) ||
              (uint32_t)(now-c->peers[id].hello_at)>=800 ||
              released;
    if(!away) s->away_tracking &= ~bit;
    else if(!(s->away_tracking & bit)) {
      s->away_tracking |= bit; s->away_since[id]=now;
    } else if((uint32_t)(now-s->away_since[id])>=HR_TOUCH_REARM_MS) {
      s->blocked &= ~bit;s->away_tracking &= ~bit;
    }
  }
  bool home=v->page==HR_PAGE_HOME, pair=v->page==HR_PAGE_PAIR;
  bool manual=c->social.manual, friends=v->page==HR_PAGE_FRIENDS && manual;
  bool ready=(home||pair||friends) && !v->activation_required && !v->setup &&
             !v->intro_kind && !v->phone_connected && c->hardware_ok &&
             !c->held && !c->speaker;
  if(!ready) {
    if(c->social.active && ((s->background && !pair) || v->activation_required ||
       (manual && (v->phone_connected || !c->hardware_ok || v->setup || v->intro_kind))))
      hr_social_leave(c);
    s->background=false;reset_match(s);proposal(c,0,now);
    return false;
  }
  if(home) {
    if(!c->social.active) hr_social_enter(c);
    s->background=true;
  } else if(friends) {
    s->background=true;
  } else {
    s->background=false;
    if(c->social.candidate) s->blocked |= 1u<<c->social.candidate;
  }
  if(!neutral) {reset_match(s);proposal(c,0,now);return false;}
  unsigned best=0,runner=0;int second=-128*256;
  for(unsigned id=1;id<=HR_PLAYERS;id++) {
    if(!available(c,id,now)) continue;
    if(manual && pair && id!=c->social.locked_candidate)continue;
    int signal=c->peers[id].signal_q8;
    if(!best || signal>c->peers[best].signal_q8) {
      if(best) {second=c->peers[best].signal_q8;runner=best;}
      best=id;
    } else if(signal>second) {second=signal;runner=id;}
  }
  s->nearest=best;s->runner=runner;
  int threshold=manual?HR_SOCIAL_MANUAL_RSSI:HR_TOUCH_RSSI;
  if(!best || c->peers[best].signal_q8<threshold*256) {
    s->reason=HR_TOUCH_WEAK;reset_match(s);proposal(c,0,now);return false;
  }
  if((home && ((s->blocked|c->social.friends)&(1u<<best))) ||
     (pair && best!=c->social.candidate)) {
    s->reason=HR_TOUCH_BLOCKED;reset_match(s);proposal(c,0,now);return false;
  }
  if(c->peers[best].signal_q8-second<(manual?HR_MANUAL_MARGIN:HR_TOUCH_MARGIN)*256) {
    s->reason=HR_TOUCH_AMBIGUOUS;reset_match(s);proposal(c,0,now);return false;
  }
  hr_peer_t *p=&c->peers[best];
  // Require the latest three raw samples too: the smoothed value alone can
  // remain high briefly after the badges move apart.
  for(unsigned i=0;i<3;i++)
    if(p->signal_window[i] < threshold-4) {
      s->reason=HR_TOUCH_WEAK;reset_match(s);proposal(c,0,now);return false;
    }
  s->reason=HR_TOUCH_DWELL;
  if(s->candidate!=best || s->peer_boot!=p->boot) {
    s->candidate=best;s->since=now;s->samples=p->samples;
    s->mutual_tracking=false;s->peer_boot=p->boot;proposal(c,0,now);return false;
  }
  /* Require repeated reciprocal intentions, not one cached HELLO emitted
   * before the other badge turned toward a different person. */
  if(p->touch_target!=c->id)s->mutual_tracking=false;
  else if(!s->mutual_tracking || s->mutual_session!=p->social_session) {
    s->mutual_tracking=true;s->mutual_since=now;
    s->mutual_samples=p->samples;s->mutual_session=p->social_session;
  }
  if((uint32_t)(now-s->since)<(manual?HR_MANUAL_MS:HR_TOUCH_MS) ||
     (uint32_t)(p->samples-s->samples)<(manual?HR_MANUAL_SAMPLES:HR_TOUCH_SAMPLES)) return false;
  proposal(c,best,now);
  // Both badges independently need a stable signal above their mode's threshold.
  // This is a discovery proposal only; neither side saves a friend here.
  s->reason=HR_TOUCH_MUTUAL;
  if(!(home||friends) || !s->mutual_tracking ||
     (uint32_t)(now-s->mutual_since)<HR_TOUCH_MUTUAL_MS ||
     (uint32_t)(p->samples-s->mutual_samples)<HR_TOUCH_MUTUAL_SAMPLES-1) return false;
  c->social.candidate=best;
  c->social.locked_candidate=best;
  v->page=HR_PAGE_PAIR;s->reason=HR_TOUCH_OPENED;
  s->blocked |= 1u<<best;s->away_tracking &= ~(1u<<best);
  s->background=false;
  return true;
}
