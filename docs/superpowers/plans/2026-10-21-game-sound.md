# Game sound implementation

User authorized death, victory and synchronized 3/2/1 cues, without disrupting the game. Implement inline; no new approval gate or commit.

- Add a small integer PCM synthesizer: countdown 70/70/100ms, descending death 180ms, ascending victory 500ms, attack/release envelope and peak <=6000. No sound assets, heap, new task or queue.
- Add one-shot game event selection and a latest-request mailbox, consumed by the existing 20ms playback worker. Leaving the game, phone audio pause and sleep cancel pending/current effects.
- Guard microphone pitch from request until 200ms after the final generated frame. The board's 6x240 I2S DMA spans 90ms at 16kHz; the remaining tail covers acoustic decay. Continue reading/draining microphone DMA, clear pitch/window/median history during the guard, leave rendering/navigation/game simulation running.
- Keep game rules, scores, room protocol and volume 90 unchanged. Victory fits within the existing 1.8s celebration; countdown ends well before actual play begins.
- First run a failing sound-event/render/guard test, then implement. Test actual capture worker against synthesized speaker feedback, plus pause/restart cancellation and the full existing host suite. Render WAV previews from the production synthesizer and verify PCM envelope/length/levels.
- Build/size/package v0.5.6, independently rebuild source, compare images, flash exact Miao identity if connected, clear demo areas as already authorized, verify full readback and original identity/NVS, then inspect runtime and request real listening/game acceptance.

## v0.5.7 feedback adjustment

User heard countdown, no death sound, and quiet victory. Actual v056 log contains four death events with completed synthesis and no audio errors; volume 90 confirmed. Low 440-to-140 Hz death sweep and conservative PCM level are the audibility hypothesis, not an acoustic measurement. Raise death to 1200-to-550 Hz/peak12000 and victory to peak10000, preserve 90 global volume and the exact durations/echo tail. Add a central 80px digits-only countdown on an opaque card, drawn into the existing partial buffer. No new heap buffers/tasks/queues. Keep physics/scoring/protocol unchanged.

First reproduced missing central digit and weak event levels with failing host tests. Verify generated frequency/levels/headroom/envelopes, all countdown transitions, production capture with stronger death/victory feedback, production playback at 90 and cancellation, then full build/size/source/flash/readback. Actual hearing remains a user test.
