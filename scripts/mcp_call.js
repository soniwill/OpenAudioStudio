#!/usr/bin/env node
// mcp_call.js — Cliente direto do servidor MCP do OpenAudio Studio.
//
// Fala JSON-RPC 2.0 newline-delimited sobre TCP (padrão 127.0.0.1:7451),
// sem depender do catálogo MCP da sessão do OpenCode. Use quando as
// ferramentas studio_ui_* não estiverem disponíveis nativamente ou para
// validar a interface a partir de qualquer agente.
//
// Pré-requisito: OpenAudio Studio em execução com o servidor MCP ativo
// ("OpenAudio Studio.exe --mcp", ou --mcp-port=<porta> com OAE_MCP_PORT).
//
// Uso:
//   node scripts/mcp_call.js --check
//       Apenas verifica a conexão. Exit 0 se o servidor responder.
//   node scripts/mcp_call.js --list
//       Lista as ferramentas disponíveis.
//   node scripts/mcp_call.js <ferramenta> [argumentos-json | @arquivo.json]
//       Chama uma ferramenta. Exemplos:
//       node scripts/mcp_call.js studio_ui_inspect "{\"max_depth\": 3}"
//       node scripts/mcp_call.js studio_ui_mouse @args.json
//   Opções:
//     --out <caminho>   Salva a resposta JSON completa em arquivo (evidências).
//     --wait <segundos> Aguarda a porta aceitar conexões antes de continuar
//                       (útil logo após abrir o Studio). Padrão: 0.
//     --timeout <ms>    Timeout de inatividade da conexão. Padrão: 30000.
//
// Variáveis de ambiente: OAE_MCP_HOST (padrão 127.0.0.1), OAE_MCP_PORT (padrão 7451).
//
// Códigos de saída: 0 = sucesso; 1 = conexão indisponível (Studio fechado?);
// 2 = erro reportado pela ferramenta ou JSON-RPC.

const net = require('net');
const fs = require('fs');

const HOST = process.env.OAE_MCP_HOST || '127.0.0.1';
const PORT = parseInt(process.env.OAE_MCP_PORT || '7451', 10);

function parseArgs(argv) {
  const opts = { out: null, wait: 0, timeout: 30000, tool: null, toolArgs: {}, flags: [] };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '--out') opts.out = argv[++i];
    else if (a === '--wait') opts.wait = parseInt(argv[++i], 10) || 0;
    else if (a === '--timeout') opts.timeout = parseInt(argv[++i], 10) || 30000;
    else if (a === '--list' || a === '--check') opts.flags.push(a);
    else if (!opts.tool && !a.startsWith('--')) opts.tool = a;
    else if (opts.tool && !a.startsWith('--')) opts.toolArgs = a;
  }
  if (typeof opts.toolArgs === 'string') {
    if (opts.toolArgs.startsWith('@')) {
      opts.toolArgs = JSON.parse(fs.readFileSync(opts.toolArgs.slice(1), 'utf8').replace(/^\uFEFF/, ''));
    } else {
      opts.toolArgs = JSON.parse(opts.toolArgs);
    }
  }
  return opts;
}

function connectOnce() {
  return new Promise((resolve) => {
    const s = net.createConnection({ host: HOST, port: PORT });
    s.once('connect', () => { s.destroy(); resolve(true); });
    s.once('error', () => resolve(false));
  });
}

async function waitForPort(seconds) {
  const deadline = Date.now() + seconds * 1000;
  while (Date.now() < deadline) {
    if (await connectOnce()) return true;
    await new Promise((r) => setTimeout(r, 500));
  }
  return await connectOnce();
}

function fail(code, msg) {
  console.error(msg);
  process.exit(code);
}

async function main() {
  const opts = parseArgs(process.argv.slice(2));
  const check = opts.flags.includes('--check');
  const list = opts.flags.includes('--list');

  if (opts.wait > 0 && !(await waitForPort(opts.wait))) {
    fail(1, `[mcp_call] Porta ${HOST}:${PORT} inacessivel apos ${opts.wait}s. Abra o Studio com --mcp (ou defina OAE_MCP_PORT).`);
  }

  const sock = net.createConnection({ host: HOST, port: PORT });
  sock.setNoDelay(true);
  let buf = '';
  const pending = new Map();
  let nextId = 1;

  const inactivity = setTimeout(() => {
    fail(1, `[mcp_call] Timeout de ${opts.timeout}ms sem resposta em ${HOST}:${PORT}.`);
  }, opts.timeout);
  inactivity.unref();

  function send(method, params) {
    return new Promise((resolve, reject) => {
      const id = nextId++;
      pending.set(id, { resolve, reject });
      sock.write(JSON.stringify({ jsonrpc: '2.0', id, method, params }) + '\n');
    });
  }

  sock.on('data', (chunk) => {
    buf += chunk.toString('utf8');
    let idx;
    while ((idx = buf.indexOf('\n')) >= 0) {
      const line = buf.slice(0, idx).trim();
      buf = buf.slice(idx + 1);
      if (!line) continue;
      let msg;
      try { msg = JSON.parse(line); } catch { continue; }
      if (msg.id && pending.has(msg.id)) {
        const p = pending.get(msg.id);
        pending.delete(msg.id);
        inactivity.refresh();
        p.resolve(msg);
      }
    }
  });

  sock.on('error', (e) => {
    fail(1, `[mcp_call] Falha de conexao com ${HOST}:${PORT}: ${e.message}\n[mcp_call] Abra o Studio com --mcp antes de validar pela interface.`);
  });

  const init = await send('initialize', {
    protocolVersion: '2024-11-05',
    capabilities: {},
    clientInfo: { name: 'oas-mcp-call', version: '1.0.0' },
  });
  if (init.error) fail(2, `[mcp_call] initialize falhou: ${JSON.stringify(init.error)}`);
  const serverInfo = init.result && init.result.serverInfo;
  console.error(`[mcp_call] Conectado a ${serverInfo ? `${serverInfo.name} ${serverInfo.version || ''}` : HOST + ':' + PORT}`);

  if (check) {
    console.log('OK');
    sock.end();
    return;
  }

  let response;
  if (list) {
    response = await send('tools/list', {});
    const tools = (response.result && response.result.tools) || [];
    console.log(JSON.stringify(tools.map((t) => ({ name: t.name, description: t.description })), null, 1));
  } else {
    if (!opts.tool) fail(2, '[mcp_call] Informe a ferramenta ou --list/--check. Veja o cabecalho do script.');
    response = await send('tools/call', { name: opts.tool, arguments: opts.toolArgs });
    const out = JSON.stringify(response, null, 1);
    if (opts.out) {
      fs.writeFileSync(opts.out, out);
      console.error(`[mcp_call] Resposta completa salva em ${opts.out}`);
    }
    if (response.error) {
      console.log(out.slice(0, opts.out ? 200 : undefined));
      fail(2, `[mcp_call] Erro JSON-RPC chamando ${opts.tool}: ${JSON.stringify(response.error).slice(0, 300)}`);
    }
    const result = response.result || {};
    if (result.isError) fail(2, `[mcp_call] A ferramenta ${opts.tool} reportou isError: true. Resposta completa acima${opts.out ? ' e em ' + opts.out : ''}.`);
    console.log(out.length > 20000 && !opts.out ? out.slice(0, 20000) + '\n[mcp_call] (resposta truncada; use --out para salvar completa)' : out);
  }
  sock.end();
  setTimeout(() => process.exit(0), 500).unref();
}

main().catch((e) => fail(2, `[mcp_call] ${e.message}`));
