#!/usr/bin/env python3
import json
import zipfile
from pathlib import Path
from release_layout import validate_segments,sha
from verify_web_image import verify
from package_device_console import main as package_console
ROOT=Path(__file__).resolve().parents[1]
NAME='Passport-HideSeek-Radio-v0.5.9'

def archive(path,files):
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for name,data in sorted(files.items()):
            item=zipfile.ZipInfo(name,(2026,10,21,0,0,0));item.compress_type=zipfile.ZIP_DEFLATED;item.create_system=3;item.external_attr=(0o100755 if name.endswith('.command') else 0o100644)<<16;z.writestr(item,data)

def main():
    package_console()
    out=ROOT/'releases/v0.5.9';out.mkdir(parents=True,exist_ok=True)
    segments={'bootloader.bin':(0,(ROOT/'build/bootloader/bootloader.bin').read_bytes()),
              'partition-table.bin':(0x8000,(ROOT/'build/partition_table/partition-table.bin').read_bytes()),
              'app.bin':(0x10000,(ROOT/'build/passport_hideseek_radio.bin').read_bytes())}
    validate_segments(segments)
    web=bytearray(b'\xff'*(0x10000+len(segments['app.bin'][1])))
    for offset,data in segments.values():web[offset:offset+len(data)]=data
    webname=NAME+'-web.bin';(out/webname).write_bytes(web);report=verify(out/webname)
    files={name:data for name,(_,data) in segments.items()};files[webname]=bytes(web)
    for path in ['README.md','docs/OPERATE-AND-TEST.md','docs/MINIAPP.md','docs/verification.md','tools/safe_flash.py','tools/release_layout.py','tools/verify_release.py','tools/verify_web_image.py','assets/fonts/FONT-NOTICES.txt']:
        files[path]=(ROOT/path).read_bytes()
    sources={}
    for name in ['main','components','tests','tools','assets/fonts','assets/profiles','assets/gallery','assets/aidol','assets/game','docs']:
        for path in (ROOT/name).rglob('*'):
            if path.is_file() and '__pycache__' not in path.parts and path.suffix!='.ppm' and (path.suffix!='.html' or 'device_console' in path.parts) and (path.suffix!='.log' or path.name.endswith('-v059.log')) and (not 'live-show' in path.parts or path.name=='progress.md'):
                sources[str(path.relative_to(ROOT))]=path.read_bytes()
    for name in ['CMakeLists.txt','sdkconfig.defaults','partitions.csv','dependencies.lock','README.md','.gitignore']:
        sources[name]=(ROOT/name).read_bytes()
    source_name=NAME+'-source.zip';archive(out/source_name,sources)
    evidence_path=ROOT/'docs/evidence/device-v059.json'
    installation=json.loads(evidence_path.read_text()) if evidence_path.exists() else None
    if installation and installation.get('app_sha256')!=sha(segments['app.bin'][1]):installation=None
    manifest={'version':'0.5.9','chip':'esp32c3','idf':'5.5.3','flash_size':0x800000,
      'device_flashing_performed':bool(installation and installation.get('flashing_performed')),'physical_acceptance':'partial: user accepted normal wearing-distance voice control and improved neighboring-workstation rejection on v0.5.9; full radio/game/audio/sleep and six-hour battery regression remain pending',
      'installation_evidence':installation,
      'device_console':{'version':'0.1.0','source':'tools/device_console','package':'../device-console-v0.1.0/AI-PASSPORT-Demo-Console-v0.1.0.zip','sha256':sha((ROOT/'releases/device-console-v0.1.0/AI-PASSPORT-Demo-Console-v0.1.0.zip').read_bytes()),'data_only':True,'original_sn_retained':True,'firmware_updated_by_tool':False,'roles':['喵喵','胡彦斌','王珞丹','小狮日记','手工耿','LKs']},
      'features':['normal-wearing-distance-voice-gate','raw-pitch-diagnostics','game-close-voice-rms-hysteresis-gate','lower-baseline-and-forgiving-spike-body-hitbox','large-central-3-2-1-countdown','midrange-death-cue-and-stronger-victory','countdown-death-victory-procedural-sfx','speaker-echo-pitch-guard','sustained-pitch-height-hold-with-240ms-voicing-grace','coin-popup-and-streak-feedback','dma-budget-for-concurrent-wifi-and-audio','voice-gated-fixed-course-without-play-deadline','safe-checkpoint-rewind','high-pitch-coins' ,'white-chicken-blue-island-flash-sprite-art','water-rescue-and-finish-confetti','persistent-best-room-session-leaderboard','game-exclusive-microphone-and-local-radio-pause','automatic-room-join-animation','names-only-room-list','bottom-aligned-left-low-right-high-signal','explicit-friend-and-photo-return','cancel-touch-raw-rssi-rearm','volatile-mutual-avatar-preview-before-confirmation','aidol-profile-home','home-profile-left-aligned-hierarchy','mutual-friends','dedicated-touch-confirmation','home-entry-saved-friends','friend-card-example-qr','original-96x156-home-avatar','auto-open-added-friend-card','friend-card-long-down-home','proportional-paged-friend-list','proximity','half-duplex-ptt','miniapp-profile-text-and-png','peer-profile-text','explicit-room-membership','offline-friend-avatar-cards','miniapp-jpeg-album','10min-deep-sleep','first-frame-backlight-fade','physical-down-return','up-center-navigation-down-confirm','join-animation-and-members-only','non-scannable-album-example','original-aidol-first-welcome-audio','jd-joy-boot-wake-sleep-animation','direct-radio-after-join','empty-room-no-targets','home-proximity-auto-prompt-with-mutual-confirmation','persistent-first-down-key-activation','reciprocal-very-close-touch-intent'],
      'miniapp':{'service':'54524145-4341-5244-0000-000000000000','identity_version':1,'identity_secret_slot':'zero','avatar_format':'96x156 RGB/RGBA/grayscale 8-bit noninterlaced PNG','ptt_while_connected':False,'custom_peer_avatar':'mutual-confirmation snapshot transfer; persistent 64x104 portrait and 32x32 thumbnail'},
      'profile_store':{'address':0x380000,'size':0x30000,'scheme':'two atomic 64KB banks plus upload scratch'},
      'friend_store':{'address':0x3b0000,'size':0x30000,'scheme':'six slots, two atomic 16KB banks each'},
      'gallery_store':{'address':0x400000,'size':0xc0000,'scheme':'two atomic 256KB banks plus upload scratch','format':'baseline JPEG <=240x320, dimensions multiples of 8'},
      'power':{'idle_ms':600000,'mode':'deep sleep, offline','battery_six_hours_verified':False,'goodbye_ms':1200,'fade_ms':300,'physical_power_cut_animation':False},
      'home_touch':{'rssi_dbm':-30,'strongest_margin_db':12,'dwell_ms':1800,'raw_three_min_dbm':-34,'minimum_new_samples':10,'reciprocal_intent_required':True,'release_away_ms':600,'release_raw_three_below_dbm':-34,'requires_both_confirmation':True,'physical_calibration':'pending'},
      'activation':{'marker':'demo_cfg/hideseek/activated_v1','trigger':'first short down press','persists_reboot':True,'inactive_touch_disabled':True},
      'first_welcome':{'marker':'demo_cfg/hideseek/welcome_v2','duration_samples':75264,'sample_rate':16000,'source':'assets/aidol/sources.json','saved_after_audio_completed':True},
      'profile_assets':{'originals':1,'rgb565_images':4,'resident_in_flash':True,'manifest':'assets/profiles/live-manifest.json'},
      'game':{'audio_effects':{'close_voice_gate':{'rms_open':300,'rms_close':180,'voiced_windows':2,'window_ms':40,'dc_removed':True,'radio_gain_changed':False},'countdown_ms':[70,70,100],'death_ms':180,'victory_ms':500,'pcm_peak_limit':12000,'echo_guard_tail_ms':200,'new_tasks':0,'new_queues':0,'new_heap_buffers':0,'preview':'docs/evidence/sfx-v059/manifest.json','physical_listening_acceptance':'pending'},'duration_ms':None,'preparation_ms':3000,'celebration_ms':1800,'render_target_fps':25,'version':6,'finite_course_px':1000,'finish_condition':'voiced progress reaches flag','voiced_speed_px_per_second':50,'stop_grace_ms':100,'height_grace_ms':240,'checkpoint_rewind_on_death':True,'collected_coins_respawn':False,'high_coin_min_baseline_percent':165,'high_coins':4,'normal_coin_score':50,'high_coin_score':100,'clear_score':100,'maximum_score':900,'art_bytes':json.loads((ROOT/'assets/game/manifest.json').read_text())['packed_bytes'],'death_penalty':50,'minimum_score':0,'immunity_ms':1100,'best_saved_at':'actual finish before celebration','control':'voice -> forward, relative pitch -> held height; silence stops and falls; baseline calibration','song_theme':None,'level_description':'original high-low free-humming level; no specific song, transcription or lyric recognition','coins':12,'obstacles':2,'max_height_px':124,'river_y':292,'radio_paused_locally':True,'room_membership_retained':True,'coordinator_arbitration_retained':True,'best_marker':'demo_cfg/hideseek/best_v1','record':'16 bytes HRS6, owner ID and CRC32; rejects previous game rules','leaderboard':'online room members with valid persistent best; shared rank for ties','score_room_session_bound':True,'physical_microphone_and_smoothness':'pending'},
      'default_playback_volume':90,'volume_initialization_marker':'demo_cfg/hideseek/volume90_v1',
      'players':6,'default_names':['喵喵','2号设备','3号设备','4号设备','5号设备','6号设备'],'names_editable':True,
      'transport':{'type':'ESP-NOW','channel':6,'router_required':False,'coordinator_id':1,'encrypted':False},
      'protected_identity':{'start':0x356000,'size':0x4000},
      'runtime_settings_write':{'partition':'demo_cfg','start':0x360000,'size':0x6000},
      'segments':{name:{'address':address,'size':len(data),'sha256':sha(data),'erase_end':(address+len(data)+4095)//4096*4096} for name,(address,data) in segments.items()},
      'web':{'file':webname,'address':0,'size':len(web),'sha256':sha(web),'overwrites_original_nvs':True},
      'source':{'file':source_name,'sha256':sha((out/source_name).read_bytes())},
      'source_files':{name:sha(data) for name,data in sources.items()},
      'distance':'smoothed RSSI visualization; no meters or bearing'}
    files['manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
    for name,data in files.items():
        path=out/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    archive(out/(NAME+'-firmware.zip'),files)
    (out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p.read_bytes())}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'))
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
