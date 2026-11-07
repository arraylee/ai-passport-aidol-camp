#!/usr/bin/env python3
"""Package the Bluetooth helper and the matching source/segmented/web firmware."""
from pathlib import Path
import hashlib,json,zipfile,re
from release_layout import validate_segments,sha
from verify_web_image import verify
ROOT=Path(__file__).resolve().parents[1]
VERSION=re.search(r'set\(PROJECT_VER "([0-9.]+)"\)',(ROOT/'CMakeLists.txt').read_text())[1]
EVIDENCE_TAG='v'+VERSION.replace('.','')
HELPER_VERSION=re.search(r'v([0-9.]+)',(ROOT/'tools/device_console/README.md').read_text())[1]
NAME='AI-PASSPORT-BLE-Config-v'+VERSION

def archive(path,files):
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for name,data in sorted(files.items()):
   i=zipfile.ZipInfo(name,(2026,10,22,0,0,0));i.compress_type=zipfile.ZIP_DEFLATED;i.create_system=3;i.external_attr=(0o100755 if name.endswith('.command') else 0o100644)<<16;z.writestr(i,data)

def main():
 out=ROOT/'releases'/('v'+VERSION);out.mkdir(parents=True,exist_ok=True)
 segments={'bootloader.bin':(0,(ROOT/'build/bootloader/bootloader.bin').read_bytes()),'partition-table.bin':(0x8000,(ROOT/'build/partition_table/partition-table.bin').read_bytes()),'app.bin':(0x10000,(ROOT/'build/passport_hideseek_radio.bin').read_bytes())};validate_segments(segments)
 web=bytearray(b'\xff'*(0x10000+len(segments['app.bin'][1])))
 for addr,b in segments.values():web[addr:addr+len(b)]=b
 webname=NAME+'-web.bin';(out/webname).write_bytes(web);verify(out/webname)
 helper={}
 for p in (ROOT/'tools/device_console').rglob('*'):
  if p.is_file() and '__pycache__' not in p.parts and p.name not in ['console_transport.py','console_core.py','nvs_identity.py','build_presets.py'] and p.suffix!='.bin' and (not 'tests' in p.parts or p.name.startswith('test_ble') or p.name in ('test_server.py','test_queue.py','test_instance.py')):
   helper[str(p.relative_to(ROOT/'tools/device_console'))]=p.read_bytes()
 helpername='AI-PASSPORT-Bluetooth-Console-v'+HELPER_VERSION+'.zip';archive(out/helpername,helper)
 source={}
 for directory in ('main','components','tests','tools','assets','docs'):
  for p in (ROOT/directory).rglob('*'):
   if p.is_file() and '__pycache__' not in p.parts and '.venv' not in p.parts and ('evidence' not in p.parts or p.name=='release-'+EVIDENCE_TAG+'.json' or p.name.endswith(('-v077.json','-v077.log','-v078.json','-v078.log','-v079.json','-v079.log','-v0710.json','-v0710.log')) or p.name.startswith(('v077-','v078-','v079-','v0710-','manual-v0710-'))) and p.suffix!='.ppm' and (p.suffix!='.log' or p.name.endswith(('-v077.log','-v078.log','-v079.log','-v0710.json','-v0710.log'))):
    source[str(p.relative_to(ROOT))]=p.read_bytes()
 for name in ('CMakeLists.txt','sdkconfig.defaults','partitions.csv','dependencies.lock','.gitignore','README.md','LICENSE','THIRD_PARTY_NOTICES.md'):
  source[name]=(ROOT/name).read_bytes()
 sourcename=NAME+'-source.zip';archive(out/sourcename,source)
 manifest={'version':VERSION,'chip':'esp32c3','idf':'5.5.3','flash_size':0x800000,'device_flashing_performed':False,'physical_acceptance':'pending: host tests and firmware build do not establish live multi-device/audio/game acceptance','protected_identity':{'start':0x356000,'size':0x4000},'segments':{name:{'address':addr,'size':len(b),'sha256':sha(b)} for name,(addr,b) in segments.items()},'web':{'file':webname,'address':0,'size':len(web),'sha256':sha(web),'overwrites_original_nvs':True},'source':{'file':sourcename,'sha256':sha((out/sourcename).read_bytes())},'source_files':{name:sha(b) for name,b in source.items()},'helper':{'file':helpername,'version':HELPER_VERSION,'sha256':sha((out/helpername).read_bytes()),'transport':'native BLE','max_batch':6,'parallel_connections':2},'manual_friends':{'entry':'open Friends on both devices','rssi_dbm':-65,'dominance_db':3,'dwell_ms':600,'new_samples':3,'reciprocal_ms':300,'reciprocal_hellos':2,'both_confirm':True,'known_friend_refresh':True,'cancel_reopen_retries':True,'hello_reserved_flag':128,'both_need_version':'0.7.10','physical_acceptance':'pending'},'touch':{'absolute_rssi_dbm':-30,'dominance_db':6,'dwell_ms':1800,'new_samples':10,'reciprocal_ms':300,'reciprocal_hellos':2,'both_confirm':True,'physical_calibration':'pending'},'room_ui':{'all_peer_names_visible':True,'game_status_above_row_backgrounds':True,'max_choices':8,'peer_count_header':True,'order':['all','game','back','peers'],'all_label':'和全体队友对讲'},'venue':{'style':'Aidol navy/chrome tech stage','reference':'assets/game/venue/aidol-reference.png','quiet_player_column':True,'hazard':'red core with pale metallic spikes','geometry_changed':False},'character':{'name':'JOY','frames':4,'size':[56,56],'poses':['idle','push','tuck','land'],'official_reference':'https://jdrdl.jd.com/Brand-ip.html','asset_status':'generated adaptation, not official game asset or brand approval','game_physics_changed':'only spike death respawn/checkpoint'},'battery':{'source':'CW2017 SOC','home_only':True,'numeric_percent':False,'poll_ms':5000,'unknown_retry_ms':1000,'service_i2c_timeout_ms':5,'invalid_read_keeps_last_valid_ms':30000,'automatic_empty_profile_repair':True,'profile_source':'Aidol CW2017_PROFILE_SEL=1 4.2v_520mah','profile_bytes':80,'write_readback_verify':True,'committed_nonempty_calibration_preserved':True,'flash_data_touched':False},'cards':{'saved_snapshot_resume':True,'window_chunks':4,'reorder_buffer_bytes':656,'minimum_send_interval_ms':5,'ack_retry_ms':80,'roster_retry_ms':500,'local_verified_card_commit_before_image_ack':True,'progress_requires_both_saved':True,'matching_snapshot_skips_flash':True,'retry_ms':80,'no_progress_timeout_ms':6000,'total_timeout_ms':20000,'failure_down_key_retry':True,'multiline_signature_lf_supported':True},'power':{'idle_ms':1200000},'wire_protocol':'HSW2 compatible with0.7.0','configuration':{'api':1,'status_bytes':228,'no_device_reboot':True,'preserves_original_nvs':True,'preserves_original_identity':True,'clear_keeps_current_persona':True},'live_evidence':'v0.7.6 live trace: held key latched blocked after 373/509ms, coordinator HELLO gaps up to1192ms. Production core RED tests reproduce HELLO starvation and stuck blocked after lost grants. v0.7.10 changes send scheduling/recovery and safe post-spike checkpoint; final hardware acceptance pending', 'result_navigation':{'options':['retry','rank','room'],'rank_return':'room','keep_membership':True,'resume_radio':True},'radio_recovery':{'fair_deadlines':True,'active_presence_ms':500,'same_press_fresh_grant_recovery':True,'lease_ms':450,'host_lease_ms':700,'max_press_ms':30000,'wire_format_changed':False},'spike_respawn':{'direction':'right','safe_platform':True,'checkpoint_updated':True,'death_penalty':50,'coins_respawn':False}}
 evidence_path=ROOT/'docs/evidence'/('release-'+EVIDENCE_TAG+'.json')
 if evidence_path.exists():
  evidence=json.loads(evidence_path.read_text())
  for key in ('device_flashing_performed','physical_acceptance','device_write_verified','boot_confirmed'):
   if key in evidence:manifest[key]=evidence[key]
  manifest['validation_evidence']='docs/evidence/release-'+EVIDENCE_TAG+'.json'
  manifest['live_evidence']=evidence.get('physical_acceptance',manifest['live_evidence'])
 manifest['home_nickname']={'width_px':108,'jd_name':'京东3C数码','jd_measured_width_px':106,'font_px':20,'native_static_acceptance':True,'physical_acceptance':'pending'}
 files={name:b for name,(_,b) in segments.items()};files[webname]=bytes(web)
 files['manifest.json']=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode()
 files['FONT-NOTICES.txt']=(ROOT/'assets/fonts/FONT-NOTICES.txt').read_bytes()
 files['LXGW-v1.522-OFL.txt']=(ROOT/'assets/fonts/LXGW-v1.522-OFL.txt').read_bytes()
 files['刷机与使用说明.txt']=('Aidol demo v'+VERSION+'\n昵称显示：京东3C数码，首页20px昵称区108px，完整静止一行。需升级本版固件以应用布局修正。\n\n官网选择 '+webname+'，起始地址0x0；不要全片擦除。原SN保护区不写入；完整网页BIN会覆盖原普通NVS。保留全部现有资料时使用分段包和safe_flash.py。\n本版新增：双方从首页打开“好友”，自动寻找同样打开好友的人；互选最近候选后，双方下键确认。取消后重新打开好友即可重试，已有好友也可更新。双方需要升级v0.7.10，单方新版无法手动匹配旧版。原自动碰一碰与好友列表保留。\n组队列表“游戏中”与远近文字置于行背景上层，游戏结束恢复信号显示。\n沿用修复：无线发送变慢时HELLO在线广播饿死对讲续期；许可超时立刻停音，按键仍按住时可等待新鲜匹配授权自动恢复，不延长占麦许可，不打断其他正在发言的人。松手停止，单次按住30秒上限保留。五六台建议统一升级，至少发言端和当前协调端都要更新才能验证完整调度修复。\n游戏：刺球死亡仍扣50分，复活在该球右侧安全平台并更新检查点，避免反复撞同一球；已拿金币不再生成，落水仍返回最近安全点。\n保留：名片多行签名与4片窗口传输、原头像、小程序修改资料/相册、JOY游戏及声音、20分钟休眠、90%默认音量、电量参数自动补齐、蓝牙配置助手v'+HELPER_VERSION+'。分区不变。\n验证：见docs/verification.md、docs/MANUAL-FRIENDS.md及manifest.json。旧版实机验收不等于本版；本版实机效果、五六台同时对讲、游戏手感和20分钟休眠/续航需要另行核实。\n').encode()
 for name,b in files.items():(out/name).write_bytes(b)
 archive(out/(NAME+'-firmware.zip'),files)
 (out/'SHA256SUMS.txt').write_text(''.join(f'{sha(p.read_bytes())}  {p.name}\n' for p in sorted(out.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'))
 print(json.dumps({'release':str(out),'web_bytes':len(web),'source_files':len(source),'helper_files':len(helper),'app_sha256':sha(segments['app.bin'][1])},indent=2))
if __name__=='__main__':main()
