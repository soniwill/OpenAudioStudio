#!/usr/bin/env node
// tutorial_openscreen_director.js — Orquestrador MCP em tempo real para gravações OpenScreen do OpenAudio Studio.
//
// Suporta dois modos:
//   --pre-setup --schedule <schedule.json> [--wait 20]
//   --record-and-direct --openscreen <exe> --project <out.openscreen> --duration <sec>
//       --schedule <schedule.json> --telemetry <telemetry.json> [--capture-frames-dir <dir>] [--wait 20]

const { spawn } = require('child_process');
const fs = require('fs');
const net = require('net');
const path = require('path');

function printHelp() {
  console.log(`tutorial_openscreen_director.js — Diretor MCP em tempo real para gravação automatizada

Uso:
  node scripts/tutorial_openscreen_director.js --pre-setup --schedule <schedule.json> [--wait 20]
  node scripts/tutorial_openscreen_director.js --record-and-direct --openscreen <exe> --project <out.openscreen> --duration <sec> --schedule <schedule.json> --telemetry <telemetry.json> [--capture-frames-dir <dir>] [--wait 20]`);
}

function parseArgs(argv) {
  const opts = {
    help: false,
    preSetup: false,
    recordAndDirect: false,
    schedule: null,
    telemetry: null,
    openscreen: null,
    project: null,
    duration: null,
    captureFramesDir: null,
    wait: 20,
    timeout: 30000
  };

  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-h' || a === '--help') opts.help = true;
    else if (a === '--pre-setup') opts.preSetup = true;
    else if (a === '--record-and-direct') opts.recordAndDirect = true;
    else if (a === '--schedule') opts.schedule = argv[++i];
    else if (a === '--telemetry') opts.telemetry = argv[++i];
    else if (a === '--openscreen') opts.openscreen = argv[++i];
    else if (a === '--project') opts.project = argv[++i];
    else if (a === '--duration') opts.duration = parseFloat(argv[++i]);
    else if (a === '--capture-frames-dir') opts.captureFramesDir = argv[++i];
    else if (a === '--wait') opts.wait = parseInt(argv[++i], 10) || 20;
    else if (a === '--timeout') opts.timeout = parseInt(argv[++i], 10) || 30000;
    else throw new Error(`Argumento desconhecido: ${a}`);
  }
  return opts;
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function readJson(filePath) {
  const raw = fs.readFileSync(filePath, 'utf8').replace(/^\uFEFF/, '');
  return JSON.parse(raw);
}

function connectOnce(host, port) {
  return new Promise((resolve) => {
    const s = net.createConnection({ host, port });
    s.once('connect', () => { s.destroy(); resolve(true); });
    s.once('error', () => resolve(false));
  });
}

async function waitForPort(host, port, seconds) {
  const deadline = Date.now() + seconds * 1000;
  while (Date.now() < deadline) {
    if (await connectOnce(host, port)) return true;
    await sleep(400);
  }
  return await connectOnce(host, port);
}

async function createMcpClient(host, port, waitSeconds, timeoutMs) {
  if (waitSeconds > 0 && !(await waitForPort(host, port, waitSeconds))) {
    throw new Error(`Porta MCP ${host}:${port} inacessivel apos ${waitSeconds}s.`);
  }

  const sock = net.createConnection({ host, port });
  sock.setNoDelay(true);
  let buf = '';
  const pending = new Map();
  let nextId = 1;
  let queue = Promise.resolve();

  await new Promise((resolve, reject) => {
    sock.once('connect', resolve);
    sock.once('error', (err) => reject(new Error(`Falha TCP ${host}:${port}: ${err.message}`)));
  });

  const inactivity = setTimeout(() => {
    sock.destroy();
  }, Math.max(timeoutMs, 120000));
  inactivity.unref();

  sock.on('data', (chunk) => {
    buf += chunk.toString('utf8');
    let idx;
    while ((idx = buf.indexOf('\n')) >= 0) {
      const line = buf.slice(0, idx).trim();
      buf = buf.slice(idx + 1);
      if (!line) continue;
      let msg;
      try {
        msg = JSON.parse(line);
      } catch {
        continue;
      }
      if (msg.id && pending.has(msg.id)) {
        const entry = pending.get(msg.id);
        pending.delete(msg.id);
        inactivity.refresh();
        entry.resolve(msg);
      }
    }
  });

  sock.on('error', (err) => {
    for (const entry of pending.values()) {
      entry.reject(err);
    }
    pending.clear();
  });

  function sendRaw(method, params) {
    return new Promise((resolve, reject) => {
      const id = nextId++;
      const timer = setTimeout(() => {
        if (pending.has(id)) {
          pending.delete(id);
          reject(new Error(`Timeout aguardando resposta JSON-RPC de ${method}`));
        }
      }, timeoutMs);
      pending.set(id, {
        resolve: (val) => { clearTimeout(timer); resolve(val); },
        reject: (err) => { clearTimeout(timer); reject(err); }
      });
      sock.write(JSON.stringify({ jsonrpc: '2.0', id, method, params }) + '\n');
    });
  }

  const initRes = await sendRaw('initialize', {
    protocolVersion: '2024-11-05',
    capabilities: {},
    clientInfo: { name: 'oas-openscreen-director', version: '1.0.0' }
  });
  if (initRes.error) {
    throw new Error(`initialize falhou: ${JSON.stringify(initRes.error)}`);
  }

  function callTool(name, args = {}) {
    const task = queue.then(async () => {
      const res = await sendRaw('tools/call', { name, arguments: args });
      if (res.error) {
        throw new Error(`${name} erro JSON-RPC: ${JSON.stringify(res.error)}`);
      }
      if (res.result && res.result.isError) {
        const msg = res.result.content && res.result.content[0] ? res.result.content[0].text : 'isError: true';
        throw new Error(`${name} falhou: ${msg}`);
      }
      return res.result;
    });
    queue = task.catch(() => {});
    return task;
  }

  function close() {
    clearTimeout(inactivity);
    sock.end();
  }

  return { callTool, close };
}

function parseInspectPayload(toolResult) {
  const text = toolResult && toolResult.content && toolResult.content[0] ? toolResult.content[0].text : '';
  if (!text) return null;
  return JSON.parse(text);
}

function isLiveParamVisible(components, paramName) {
  const lbl = components.find((c) =>
    c.type === 'Label' &&
    c.visible !== false &&
    c.bounds && c.bounds.w > 0 && c.bounds.h > 0 &&
    (c.name === paramName || (typeof c.text === 'string' && c.text.startsWith(paramName)))
  );
  return Boolean(lbl);
}

function findLiveParamSlider(components, paramName) {
  const label = components.find((c) =>
    c.type === 'Label' &&
    c.visible !== false &&
    c.bounds && c.bounds.w > 0 && c.bounds.h > 0 &&
    (c.name === paramName || (typeof c.text === 'string' && c.text.startsWith(paramName)))
  );
  if (!label) {
    return null;
  }
  const slider = components.find((c) =>
    c.type === 'Slider' &&
    c.visible !== false &&
    c.bounds && c.bounds.w > 0 && c.bounds.h > 0 &&
    c.depth === label.depth &&
    Math.abs(c.bounds.y - label.bounds.y) <= 8 &&
    c.bounds.x >= label.bounds.x
  );
  return slider ? { label, slider } : null;
}

async function getRootBounds(client) {
  const res = await client.callTool('studio_ui_inspect', {
    visible_only: true,
    max_depth: 1
  });
  const payload = parseInspectPayload(res);
  return payload && payload.root_size ? payload.root_size : { width: 1920, height: 1009 };
}

async function inspectComponentBounds(client, componentId) {
  try {
    const res = await client.callTool('studio_ui_inspect', {
      component_id: componentId,
      visible_only: true,
      max_depth: 1
    });
    const payload = parseInspectPayload(res);
    if (!payload || !Array.isArray(payload.components)) return null;
    const match = payload.components.find((c) => c.component_id === componentId) || payload.components[0];
    return match && match.bounds ? match.bounds : null;
  } catch {
    return null;
  }
}

async function executeAction(client, action, telemetry, getRelMs) {
  const kind = action.type;
  const relAtStart = getRelMs();

  if (kind === 'select') {
    await client.callTool('studio_select_item', {
      type: action.itemType || 'event',
      path: action.path
    });
  } else if (kind === 'control') {
    if (typeof action.value === 'number' && Number.isFinite(action.value)) {
      await client.callTool('studio_ui_set_control', {
        component_id: action.component_id,
        value: action.value
      });
    } else {
      await client.callTool('studio_trigger_control', {
        component_id: action.component_id
      });
    }
  } else if (kind === 'ensure_live_params') {
    const paramName = action.paramName || 'CombatIntensity';
    const wantOpen = Boolean(action.open);
    const inspRes = await client.callTool('studio_ui_inspect', {
      component_id: 'timeline_view',
      visible_only: true,
      max_depth: 10
    });
    const insp = parseInspectPayload(inspRes);
    const currentlyOpen = insp && Array.isArray(insp.components)
      ? isLiveParamVisible(insp.components, paramName)
      : false;
    if (currentlyOpen !== wantOpen) {
      await client.callTool('studio_trigger_control', {
        component_id: 'btn_timeline_params_toggle'
      });
      await sleep(100);
    }
    if (wantOpen) {
      const afterRes = await client.callTool('studio_ui_inspect', {
        component_id: 'timeline_view',
        visible_only: true,
        max_depth: 10
      });
      const after = parseInspectPayload(afterRes);
      const found = after && Array.isArray(after.components) ? findLiveParamSlider(after.components, paramName) : null;
      if (found) {
        telemetry.componentBounds[`live_param_${paramName}`] = found.slider.bounds;
      }
    }
  } else if (kind === 'set_live_param') {
    const paramName = action.name || 'CombatIntensity';
    const inspRes = await client.callTool('studio_ui_inspect', {
      component_id: 'timeline_view',
      visible_only: true,
      max_depth: 10
    });
    const insp = parseInspectPayload(inspRes);
    const comps = insp && Array.isArray(insp.components) ? insp.components : [];
    let found = findLiveParamSlider(comps, paramName);
    if (!found) {
      await client.callTool('studio_trigger_control', { component_id: 'btn_timeline_params_toggle' });
      await sleep(120);
      const retryRes = await client.callTool('studio_ui_inspect', {
        component_id: 'timeline_view',
        visible_only: true,
        max_depth: 10
      });
      const retry = parseInspectPayload(retryRes);
      found = findLiveParamSlider(retry && Array.isArray(retry.components) ? retry.components : [], paramName);
    }
    if (!found) {
      throw new Error(`Slider LIVE PARAMS '${paramName}' nao encontrado em timeline_view.`);
    }
    const targetX = found.slider.bounds.x + 12;
    const targetY = found.slider.bounds.y + Math.floor(found.slider.bounds.h / 2);
    await client.callTool('studio_ui_set_control', {
      x: targetX,
      y: targetY,
      value: action.value
    });
    telemetry.componentBounds[`live_param_${paramName}`] = found.slider.bounds;
  } else if (kind === 'set_timeline_zoom') {
    const inspRes = await client.callTool('studio_ui_inspect', {
      component_id: 'timeline_view',
      type: 'Slider',
      visible_only: true,
      max_depth: 3
    });
    const insp = parseInspectPayload(inspRes);
    const comps = insp && Array.isArray(insp.components) ? insp.components : [];
    const slider = comps.find((c) =>
      c.type === 'Slider' &&
      c.visible !== false &&
      c.bounds && c.bounds.w > 0 && c.bounds.h > 0 &&
      c.min === 10 &&
      c.max === 2000
    );
    if (!slider) {
      throw new Error('Slider de zoom da timeline (min=10, max=2000) nao encontrado em timeline_view.');
    }
    const targetX = slider.bounds.x + Math.floor(slider.bounds.w / 2);
    const targetY = slider.bounds.y + Math.floor(slider.bounds.h / 2);
    await client.callTool('studio_ui_set_control', {
      x: targetX,
      y: targetY,
      value: action.pixelsPerSecond
    });
  } else if (kind === 'highlight') {
    await client.callTool('studio_ui_highlight', {
      component_id: action.component_id,
      label: action.label || '',
      dim_opacity: typeof action.dim_opacity === 'number' ? action.dim_opacity : 0.45,
      color: action.color || 'cyan'
    });
    const bounds = await inspectComponentBounds(client, action.component_id);
    if (bounds) {
      telemetry.componentBounds[action.component_id] = bounds;
    }
  } else if (kind === 'clear_highlight') {
    await client.callTool('studio_ui_highlight', { clear: true });
  } else if (kind === 'transport') {
    const act = action.action;
    const actualMs = getRelMs();
    await client.callTool('studio_transport', { action: act });
    if (act === 'play') {
      telemetry.isPlaying = true;
      if (telemetry.playStartMs === null) telemetry.playStartMs = actualMs;
    } else if (act === 'stop') {
      telemetry.isPlaying = false;
      telemetry.playStopMs = actualMs;
    }
  } else {
    throw new Error(`Tipo de acao desconhecido no schedule: '${kind}'`);
  }

  telemetry.actionLog.push({
    type: kind,
    plannedMs: typeof action.atMs === 'number' ? action.atMs : 0,
    actualMs: relAtStart,
    detail: action.component_id || action.name || action.path || action.action || (typeof action.pixelsPerSecond === 'number' ? String(action.pixelsPerSecond) : '')
  });
}

async function runPreSetup(opts) {
  const schedule = readJson(path.resolve(opts.schedule));
  const host = process.env.OAE_MCP_HOST || '127.0.0.1';
  const port = parseInt(process.env.OAE_MCP_PORT || '7451', 10);
  const client = await createMcpClient(host, port, opts.wait, opts.timeout);
  const telemetry = { componentBounds: {}, actionLog: [], playStartMs: null, playStopMs: null, isPlaying: false };
  try {
    const preActions = Array.isArray(schedule.preSetup) ? schedule.preSetup : [];
    for (const action of preActions) {
      await executeAction(client, action, telemetry, () => 0);
      await sleep(60);
    }
    const rootBounds = await getRootBounds(client);
    console.log(JSON.stringify({ ok: true, rootBounds, preSetupCount: preActions.length }));
  } finally {
    client.close();
  }
}

async function runRecordAndDirect(opts) {
  if (!opts.openscreen || !opts.project || !opts.duration || !opts.schedule || !opts.telemetry) {
    throw new Error('Parametros obrigatorios para --record-and-direct: --openscreen, --project, --duration, --schedule, --telemetry.');
  }

  const schedule = readJson(path.resolve(opts.schedule));
  const host = process.env.OAE_MCP_HOST || '127.0.0.1';
  const port = parseInt(process.env.OAE_MCP_PORT || '7451', 10);
  const client = await createMcpClient(host, port, opts.wait, opts.timeout);

  const captureDir = opts.captureFramesDir ? path.resolve(opts.captureFramesDir) : null;
  if (captureDir) {
    fs.rmSync(captureDir, { recursive: true, force: true });
    fs.mkdirSync(captureDir, { recursive: true });
  }

  const telemetry = {
    rootBounds: await getRootBounds(client),
    playStartMs: null,
    playStopMs: null,
    isPlaying: false,
    rawScreenVideoPath: null,
    componentBounds: {},
    actionLog: [],
    frames: []
  };

  let frameIndex = 0;
  let lastFrameMs = -10000;
  let t0 = null;
  let stopCapturing = false;
  let recordingDonePayload = null;
  const actions = [...(Array.isArray(schedule.actions) ? schedule.actions : [])]
    .sort((a, b) => (a.atMs || 0) - (b.atMs || 0));
  let nextActionIdx = 0;

  async function captureFrame(explicitMs = null) {
    if (!captureDir) return;
    const filename = `f_${String(frameIndex++).padStart(4, '0')}.png`;
    const fullPath = path.join(captureDir, filename);
    await client.callTool('studio_ui_screenshot', {
      output_path: fullPath,
      include_base64: false
    });
    const relMs = explicitMs !== null ? explicitMs : (t0 !== null ? Date.now() - t0 : 0);
    lastFrameMs = relMs;
    telemetry.frames.push({ file: filename, tMs: Math.max(0, relMs) });
  }

  try {
    if (captureDir) {
      await captureFrame(0);
    }

    const args = [
      'record',
      '--display', '0',
      '--system-audio',
      '--cursor', 'system',
      '--duration', String(opts.duration),
      '--project', path.resolve(opts.project),
      '--json'
    ];

    const child = spawn(opts.openscreen, args, { stdio: ['ignore', 'pipe', 'pipe'] });
    let stdoutBuf = '';
    let stderrBuf = '';
    let resolveStarted;
    let rejectStarted;
    const startedPromise = new Promise((res, rej) => {
      resolveStarted = res;
      rejectStarted = rej;
    });

    const markStarted = (reason) => {
      if (t0 === null) {
        t0 = Date.now();
        console.error(`[director] Gravacao iniciada (${reason}) em epoch=${t0}`);
        resolveStarted(t0);
      }
    };

    const handleLine = (source, line) => {
      const trimmed = line.trim();
      if (!trimmed) return;
      if (source === 'stdout') {
        try {
          const ev = JSON.parse(trimmed);
          if (ev.event === 'log' && ev.message === 'Recording started') {
            markStarted('stdout:Recording started');
          } else if (ev.event === 'log' && typeof ev.message === 'string' && ev.message.startsWith('Duration reached')) {
            stopCapturing = true;
          } else if (ev.event === 'done') {
            stopCapturing = true;
            recordingDonePayload = ev;
          }
        } catch {
          // linha nao JSON ignorada
        }
      } else {
        if (trimmed.includes('[native-wgc] capture started')) {
          markStarted('stderr:native-wgc capture started');
        }
      }
    };

    child.stdout.on('data', (d) => {
      stdoutBuf += d.toString('utf8');
      let idx;
      while ((idx = stdoutBuf.indexOf('\n')) >= 0) {
        const line = stdoutBuf.slice(0, idx);
        stdoutBuf = stdoutBuf.slice(idx + 1);
        handleLine('stdout', line);
      }
    });

    child.stderr.on('data', (d) => {
      stderrBuf += d.toString('utf8');
      let idx;
      while ((idx = stderrBuf.indexOf('\n')) >= 0) {
        const line = stderrBuf.slice(0, idx);
        stderrBuf = stderrBuf.slice(idx + 1);
        handleLine('stderr', line);
      }
    });

    const childExitPromise = new Promise((resolve, reject) => {
      child.once('error', (err) => {
        if (t0 === null) rejectStarted(err);
        reject(err);
      });
      child.once('close', (code) => {
        stopCapturing = true;
        if (t0 === null) {
          rejectStarted(new Error(`Openscreen record encerrou (codigo ${code}) antes de iniciar captura.`));
        }
        resolve(code || 0);
      });
    });

    const startTimeout = setTimeout(() => {
      if (t0 === null) {
        rejectStarted(new Error('Timeout (20s) aguardando Openscreen iniciar gravacao.'));
      }
    }, 20000);

    try {
      await startedPromise;
    } finally {
      clearTimeout(startTimeout);
    }

    const getRelMs = () => Date.now() - t0;
    const VISUAL_TYPES = new Set(['select', 'control', 'ensure_live_params', 'set_live_param', 'set_timeline_zoom', 'highlight', 'clear_highlight']);

    const runActionTimeline = async () => {
      for (let i = 0; i < actions.length; i++) {
        nextActionIdx = i;
        const action = actions[i];
        if (stopCapturing) break;
        const targetMs = typeof action.atMs === 'number' ? action.atMs : 0;
        const waitMs = targetMs - getRelMs();
        if (waitMs > 0) {
          await sleep(waitMs);
        }
        if (stopCapturing) break;
        await executeAction(client, action, telemetry, getRelMs);
        if (captureDir && VISUAL_TYPES.has(action.type) && !stopCapturing) {
          await sleep(80);
          if (!stopCapturing) {
            await captureFrame();
          }
        }
      }
      nextActionIdx = actions.length;
    };

    const runBackgroundFrames = async () => {
      if (!captureDir) return;
      const maxDurationMs = Math.round(opts.duration * 1000);
      while (!stopCapturing && getRelMs() < maxDurationMs) {
        const rel = getRelMs();
        const upcomingMs = nextActionIdx < actions.length ? (actions[nextActionIdx].atMs || 0) : Infinity;
        const minInterval = telemetry.isPlaying ? 25 : 600;
        if ((upcomingMs - rel) > 130 && (rel - lastFrameMs >= minInterval)) {
          try {
            await captureFrame();
          } catch (err) {
            console.error(`[director] Aviso ao capturar frame em ${rel}ms: ${err.message}`);
          }
        }
        await sleep(telemetry.isPlaying ? 25 : 80);
      }
    };

    await Promise.all([runActionTimeline(), runBackgroundFrames()]);

    const exitCode = await childExitPromise;
    if (exitCode !== 0) {
      throw new Error(`Openscreen record retornou codigo ${exitCode}.`);
    }

    if (recordingDonePayload && recordingDonePayload.screenVideoPath) {
      telemetry.rawScreenVideoPath = recordingDonePayload.screenVideoPath;
    } else if (fs.existsSync(opts.project)) {
      const proj = readJson(opts.project);
      if (proj.media && proj.media.screenVideoPath) {
        telemetry.rawScreenVideoPath = proj.media.screenVideoPath;
      }
    }

    const telemetryPath = path.resolve(opts.telemetry);
    fs.mkdirSync(path.dirname(telemetryPath), { recursive: true });
    fs.writeFileSync(telemetryPath, JSON.stringify(telemetry, null, 2), 'utf8');
    console.log(JSON.stringify({
      ok: true,
      framesCaptured: telemetry.frames.length,
      actionsExecuted: telemetry.actionLog.length,
      playStartMs: telemetry.playStartMs,
      playStopMs: telemetry.playStopMs,
      rawScreenVideoPath: telemetry.rawScreenVideoPath
    }));
  } finally {
    client.close();
  }
}

async function main() {
  const opts = parseArgs(process.argv.slice(2));
  if (opts.help) {
    printHelp();
    return;
  }
  if (opts.preSetup) {
    if (!opts.schedule) throw new Error('--pre-setup exige --schedule <arquivo.json>');
    await runPreSetup(opts);
    return;
  }
  if (opts.recordAndDirect) {
    await runRecordAndDirect(opts);
    return;
  }
  printHelp();
  process.exitCode = 1;
}

main().catch((err) => {
  console.error(`[tutorial_openscreen_director] ERRO: ${err.message}`);
  process.exit(1);
});
