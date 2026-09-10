// Node harness: runs the wasm poster module headless and writes the PNG,
// so it can be compared against the native Qt CLI output (pixel parity).
const fs = require('fs');
const path = require('path');
const createModule = require('../web/deploy/fh2poster.js');

function read(p) { return new Uint8Array(fs.readFileSync(p)); }

(async () => {
  const module = await createModule();
  const [save, agg, aggX] = [
    process.argv[2],
    process.argv[3],
    process.argv[4],
  ];
  const opts = process.argv[6] || JSON.stringify({ fog: 'auto', scale: 2, layout: 'cardushe', routes: 'all' });
  const info = module.renderPoster(read(save), read(agg), aggX ? read(aggX) : new Uint8Array(0), opts);
  console.log("RAW:"+info); const obj = JSON.parse(info);
  console.log(info);
  if (!obj.ok) process.exit(2);
  const png = Buffer.from(module.takeLastPng());
  const out = process.argv[5] || '/tmp/web_poster.png';
  fs.writeFileSync(out, png);
  console.log('wrote', out, png.length, 'bytes', obj.px);
})().catch(e => { console.error('FATAL', e); process.exit(1); });
