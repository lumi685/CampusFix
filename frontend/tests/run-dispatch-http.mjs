// Owns all processes/files. Overrides only this test Vite instance's proxy;
// production vite.config.js and any user server/database are untouched.
import assert from 'node:assert/strict'
import { spawn, spawnSync } from 'node:child_process'
import { mkdtempSync, readFileSync, writeFileSync, rmSync, existsSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { join, resolve, sep, dirname } from 'node:path'
import { fileURLToPath } from 'node:url'
import net from 'node:net'
import { createServer } from 'vite'
const frontend = resolve(dirname(fileURLToPath(import.meta.url)), '..'), root = resolve(frontend, '..')
const binary = join(root, 'backend/build/Debug/campusfix_backend.exe'), fixture = join(root, 'backend/build/Debug/campusfix_dispatch_ui_fixture.exe'), dispatchFixture = join(root, 'backend/build/Debug/campusfix_dispatch_http_fixture.exe')
for(const path of [binary,fixture,dispatchFixture])assert.ok(existsSync(path),'Build backend and test fixtures first')
const dir = mkdtempSync(join(tmpdir(),'campusfix-w6d5-')), db = join(dir,'test.sqlite3'), secrets = join(dir,'credentials.json'), children = [], logs = [], ports = []
let vite
const pause = ms => new Promise(r=>setTimeout(r,ms))
async function freePort(){const s=net.createServer();await new Promise(r=>s.listen(0,'127.0.0.1',r));const p=s.address().port;await new Promise(r=>s.close(r));return p}
async function stop(p){if(p.exitCode===null&&p.signalCode===null){const done=new Promise(r=>p.once('exit',r));p.kill();await done}}
try {
  const init=spawnSync(fixture,['--init',db,secrets,join(root,'backend/migrations'),join(root,'backend/seeds/001_demo_data.sql')],{encoding:'utf8',windowsHide:true});assert.equal(init.status,0,'fixture provisioning')
  const backendPort=await freePort();ports.push(backendPort)
  const backend=spawn(binary,[],{env:{...process.env,CAMPUSFIX_DB_PATH:db,CAMPUSFIX_PORT:String(backendPort)},windowsHide:true,stdio:['ignore','pipe','pipe']});children.push(backend)
  backend.stdout.on('data',c=>logs.push(c.toString()));backend.stderr.on('data',c=>logs.push(c.toString()))
  let started=false
  for(let i=0;i<100;i++){try{const r=await fetch(`http://127.0.0.1:${backendPort}/api/health`);if(r.status===200){started=true;break}}catch{}assert.equal(backend.exitCode,null,'backend alive');await pause(30)}
  assert.ok(started,'backend startup')
  vite=await createServer({configFile:join(frontend,'vite.config.js'),root:frontend,server:{host:'127.0.0.1',port:0,strictPort:true,proxy:{'/api':{target:`http://127.0.0.1:${backendPort}`}}},logLevel:'warn'})
  await vite.listen();const port=vite.httpServer.address().port;ports.push(port);const base=`http://127.0.0.1:${port}`
  const test=spawn(process.execPath,['--test',join(frontend,'tests/dispatch-http.test.js')],{env:{...process.env,CAMPUSFIX_DISPATCH_TEST_BASE:base,CAMPUSFIX_DISPATCH_TEST_CREDENTIALS:secrets,CAMPUSFIX_DISPATCH_TEST_DB:db,CAMPUSFIX_DISPATCH_TEST_FIXTURE:dispatchFixture,CAMPUSFIX_DISPATCH_UI_FIXTURE:fixture,CAMPUSFIX_DISPATCH_TEST_EVIDENCE:join(root,'backend/build/Testing/w6d5-frontend-evidence.json')},windowsHide:true,stdio:'inherit'})
  children.push(test);const status=await new Promise(r=>test.once('exit',r));assert.equal(status,0,'real frontend proxy tests must pass')
  const creds=JSON.parse(readFileSync(secrets,'utf8'));for(const secret of [...Object.values(creds.passwords),...Object.values(creds).filter(v=>typeof v==='string')])assert.equal(logs.join('').includes(secret),false,'server logs do not expose fixture passwords/sessions')
  if(process.argv.includes('--manual')) {
    const runtime=join(root,'backend/build/Testing/w6d5-browser-runtime.json');writeFileSync(runtime,JSON.stringify({base,db,secrets,fixture,dispatchFixture},null,2))
    console.log('Isolated browser session ready; runtime metadata is in ignored backend/build/Testing/w6d5-browser-runtime.json. No credentials printed.')
    const stopFile=join(root,'backend/build/Testing/w6d5-browser-stop')
    rmSync(stopFile,{force:true})
    await new Promise(r=>{let timer, polling;const done=()=>{clearTimeout(timer);clearInterval(polling);r()};timer=setTimeout(done,20*60*1000);polling=setInterval(()=>{if(existsSync(stopFile))done()},250);process.once('SIGINT',done);process.once('SIGTERM',done)})
    rmSync(stopFile,{force:true})
    rmSync(runtime,{force:true})
  }
} finally {
  if(vite)await vite.close()
  for(const p of children)await stop(p)
  const path=resolve(dir);assert.ok(path.startsWith(resolve(tmpdir())+sep)&&path.includes('campusfix-w6d5-'));rmSync(path,{recursive:true,force:true})
}
for(const port of ports){const closed=await new Promise(r=>{const s=net.createConnection({host:'127.0.0.1',port});s.once('error',()=>r(true));s.once('connect',()=>{s.destroy();r(false)})});assert.equal(closed,true,'owned listener released')}
console.log('PASS owned backend/Vite processes, ports, temporary database and random credentials cleaned')
