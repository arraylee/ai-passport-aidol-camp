'use strict';
const $=id=>document.getElementById(id),token=document.querySelector('meta[name=passport-token]').content;
let state={devices:[],roles:[],bindings:{},job:null},selected=new Set(),choices=new Map(),replacements=new Set(),renderKey='',pending=[],pendingAction='restore';
const el=(tag,text,cls)=>{const o=document.createElement(tag);if(text!==undefined)o.textContent=text;if(cls)o.className=cls;return o};
const busy=()=>state.job&&!state.job.done;
function notice(text){$('notice').hidden=!text;$('notice').textContent=text||''}
function role(id){return state.roles.find(r=>r.id===Number(id))}
function target(d){return {port:d.port,mac:d.info.mac,role:Number(choices.get(d.port)),replace:replacements.has(d.port)}}
function occupied(d,id){if(d.info?.shared_roles)return;return Object.entries(state.bindings).find(([mac,r])=>r===Number(id)&&mac!==d.info?.mac)}
async function start(action,targets){
 try{notice('');const r=await fetch('/api/start',{method:'POST',headers:{'Content-Type':'application/json','X-Passport-Token':token},body:JSON.stringify({action,targets})});const j=await r.json();if(!r.ok)throw Error(j.error||'操作未开始');state.job=j.job;renderKey='';await poll()}
 catch(e){notice(e.message)}
}
function confirmTargets(targets,action="restore"){
 if(!targets.length)return notice('请先识别并选择设备');if(action==='restore'&&targets.some(t=>!role(t.role)))return notice('请为每台设备选择目标人物');if(new Set(targets.map(t=>t.role)).size!==targets.length&&!targets.every(t=>state.devices.find(d=>d.port===t.port)?.info?.shared_roles))return notice('旧版固件请分别选择不同角色，或先升级到v0.6.0');
 for(const t of targets){const d=state.devices.find(d=>d.port===t.port);if(occupied(d,t.role)&&!t.replace)return notice('所选角色已有绑定，请确认旧设备不参与本轮并勾选替换绑定')}
 pending=targets;pendingAction=action;
 $('confirm-go').textContent=action==='clear'?'开始清空数据':'开始设置并重置';
 $('confirm-title').textContent=action==='clear'?'清空这些设备的测试数据？':'设置并重置这些设备？';
 $('confirm-desc').textContent=action==='clear'?'保留当前人物，清空好友、成绩、激活记录和自定义相册。':'恢复所选人物，并清空本轮测试状态。';
 $('confirm-caution').textContent=action==='clear'?'当前头像、昵称、签名和原 SN 保留。':'自行修改的头像、昵称、签名和相册会被替换；原 SN 保留。';$('confirm-list').replaceChildren(...targets.map(t=>{const d=state.devices.find(d=>d.port===t.port),o=el('div',undefined,'confirm-item');o.append(el('strong',action==='clear'?d.info.name:`${d.info.name} → ${role(t.role).name}`),el('br'),el('small',`SN ${d.info.sn}`));return o}));$('confirm').showModal();$('cancel').focus()
}
function render(){
 const ports=new Set(state.devices.filter(d=>d.info?.supported&&d.seen_this_scan!==false).map(d=>d.port));selected=new Set([...selected].filter(p=>ports.has(p)));
 const fresh=state.devices.filter(d=>d.seen_this_scan!==false).length,missing=state.devices.length-fresh;
 $('count').textContent=`${fresh} 台${missing?' · '+missing+' 台本轮未发现':''}`;$('empty').hidden=state.devices.length>0;
 const cards=state.devices.map(d=>{
  const info=d.info,seen=d.seen_this_scan!==false,known=!!info?.supported,id=choices.get(d.port)||0,preset=role(id);
  const card=el('article',undefined,'device'+((selected.has(d.port)?' selected':'')+(!seen?' unavailable':'')));
  const top=el('div',undefined,'device-top'),title=el('div',undefined,'device-title'),check=el('input');check.type='checkbox';check.checked=selected.has(d.port);check.disabled=!info?.supported||!seen||busy();check.setAttribute('aria-label','选择设备 '+(info?.sn||d.usb_mac||d.port));check.onchange=()=>{check.checked?selected.add(d.port):selected.delete(d.port);render()};
  title.append(check,el('h3',known?`${seen?'当前':'上次读取'}：${info.name}`:(info?'已读取 SN':'当前资料未读取')));top.append(title,el('span',!seen?'本轮未发现':info?(info.supported?'资料已读取':'旧固件 · 需升级'):'蓝牙可见','tag'));card.append(top);
  const profile=el('div',undefined,'profile');
  if(known&&info.avatar_url){const img=el('img',undefined,'portrait');img.src=info.avatar_url;img.alt=info.name+'当前头像';profile.append(img)}
  else{const placeholder=el('div','?','portrait portrait-placeholder');placeholder.setAttribute('aria-label',known?'当前头像暂不支持预览':'尚未读取设备头像');profile.append(placeholder)}
  const text=el('div',undefined,'profile-text');
  text.append(el('div',known?info.name:'当前人物未读取','name'),el('div',known?info.signature:(info?'此固件仅能读取 SN，尚不能读取当前人物资料。':'扫描后自动连接，读取设备当前资料。'),'signature'),el('div','SN '+(info?.sn||d.ble_name||d.port.slice(-8)),'sn'));
  const detail=!seen?'本轮未发现，仅保留上次资料；请唤醒设备、断开其他蓝牙连接后重新扫描。':d.read_error?'读取失败：'+d.read_error:known?`固件 v${info.version}${info.avatar_source==='builtin'?' · 固件内置头像':info.avatar_url?'':' · 昵称、签名已读取；当前头像暂不支持预览'}`:info?'升级到配套固件后可读取当前昵称和签名。':'正在读取或等待设备连接';
  text.append(el('div',detail,'firmware'));profile.append(text);card.append(profile);
  const pick=el('div',undefined,'pick'),label=el('label','目标人物'),select=el('select');select.setAttribute('aria-label','设备 '+(info?.sn||d.port)+' 的目标角色');select.disabled=busy();
  const emptyOption=el('option','请选择要设置的人物');emptyOption.value='';emptyOption.selected=!id;select.append(emptyOption);
  for(const r of state.roles){const o=el('option',r.name);o.value=r.id;o.selected=r.id===Number(id);select.append(o)}
  select.onchange=()=>{choices.set(d.port,Number(select.value));replacements.delete(d.port);render()};pick.append(label,select);card.append(pick);
  if(preset){const preview=el('div',undefined,'target-preview'),avatar=el('img');avatar.src='/presets/'+preset.avatar;avatar.alt='目标人物预览：'+preset.name;preview.append(avatar,el('span','将设置为：'+preset.name));card.append(preview)}
  const owner=info&&occupied(d,id);if(owner){const box=el('div',undefined,'replace'),lab=el('label'),c=el('input');c.type='checkbox';c.checked=replacements.has(d.port);c.disabled=busy();c.onchange=()=>c.checked?replacements.add(d.port):replacements.delete(d.port);lab.append(c,el('span',`此角色原绑定尾号 ${owner[0].slice(-5)}。确认旧设备不参与本轮，并替换绑定。`));box.append(lab);card.append(box)}
  const actions=el('div',undefined,'actions'),inspect=el('button',info?'重新识别':'识别设备'),apply=el('button','设置并重置','primary');inspect.disabled=!seen||busy();apply.disabled=!info?.supported||!seen||!id||busy();inspect.onclick=()=>start('inspect',[{port:d.port}]);apply.onclick=()=>confirmTargets([target(d)]);const clear=el('button','清空数据');clear.disabled=!info?.supported||!seen||busy();clear.onclick=()=>confirmTargets([{port:d.port,mac:info.mac,role:info.role}],'clear');actions.append(inspect,clear,apply);card.append(actions);return card
 });$('devices').replaceChildren(...cards);
 $('inspect-all').disabled=!fresh||busy();$('refresh').disabled=busy();$('select-all').disabled=!ports.size||busy();$('select-all').checked=!!ports.size&&selected.size===ports.size;$('selected-count').textContent=`已选 ${selected.size} 台`;$('apply-selected').disabled=!selected.size||busy()||[...selected].some(p=>!choices.get(p));$('clear-selected').disabled=!selected.size||busy();
 const job=state.job;$('progress').hidden=!job;if(job){$('job-title').textContent=job.done?'本次处理结果':job.action==='scan'?'正在扫描并读取当前资料':'正在通过蓝牙处理 · 请保持设备开机';$('job-items').replaceChildren(...job.items.map(i=>{const o=el('div',undefined,'job-row status-'+i.status),name=role(i.role)?.name||i.result?.name||(i.port==='scan'?'蓝牙扫描':i.port.split('/').pop());o.append(el('strong',name),el('span',i.stage));return o}))}
}
let polling=false;
async function poll(){if(polling)return;polling=true;try{const r=await fetch('/api/status');if(!r.ok)throw Error('工具连接异常');const s=await r.json();state=s;const key=JSON.stringify(s);if(key!==renderKey){renderKey=key;render()}}catch(e){notice('工具已断开，请重新双击启动助手。')}finally{polling=false}}
$('refresh').onclick=()=>start('scan',[]);$('inspect-all').onclick=()=>start('inspect',state.devices.filter(d=>d.seen_this_scan!==false).map(d=>({port:d.port})));$('select-all').onchange=()=>{selected=$('select-all').checked?new Set(state.devices.filter(d=>d.info?.supported&&d.seen_this_scan!==false).map(d=>d.port)):new Set();render()};$('apply-selected').onclick=()=>confirmTargets(state.devices.filter(d=>selected.has(d.port)&&d.info).map(target));$('cancel').onclick=()=>$('confirm').close();$('clear-selected').onclick=()=>confirmTargets(state.devices.filter(d=>selected.has(d.port)&&d.info?.supported).map(d=>({port:d.port,mac:d.info.mac,role:d.info.role})),'clear');$('confirm-go').onclick=()=>{const targets=pending;$('confirm').close();start(pendingAction,targets)};poll().then(()=>{if(!busy())start('scan',[])});setInterval(poll,1000);
