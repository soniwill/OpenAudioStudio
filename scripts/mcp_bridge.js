#!/usr/bin/env node
/**
 * OpenAudio Studio - MCP stdio-to-TCP Bridge
 * 
 * Este script faz a ponte entre o Antigravity (stdio) e o servidor TCP do
 * OpenAudio Studio (127.0.0.1:7451).
 * 
 * Se o OpenAudio Studio estiver fechado, a conexão falha imediatamente e o
 * script encerra sem forçar a abertura da janela gráfica.
 */

const net = require('net');

const HOST = process.env.OAE_MCP_HOST || '127.0.0.1';
const PORT = parseInt(process.env.OAE_MCP_PORT || '7451', 10);
const CONNECT_TIMEOUT_MS = 1500;

let isConnected = false;

const socket = new net.Socket();

socket.setTimeout(CONNECT_TIMEOUT_MS);

socket.connect(PORT, HOST, () => {
  isConnected = true;
  socket.setTimeout(0); // Desativa timeout após conexão estabelecida com sucesso
  process.stdin.pipe(socket);
  socket.pipe(process.stdout);
});

socket.on('timeout', () => {
  if (!isConnected) {
    console.error(`[openaudio-mcp-bridge] Timeout (${CONNECT_TIMEOUT_MS}ms) ao tentar conectar em ${HOST}:${PORT}. Studio não iniciado.`);
    socket.destroy();
    process.exit(1);
  }
});

socket.on('error', (err) => {
  if (!isConnected) {
    console.error(`[openaudio-mcp-bridge] Conexão com OpenAudio Studio em ${HOST}:${PORT} indisponível (${err.code || err.message}).`);
  }
  process.exit(1);
});

socket.on('close', () => {
  process.exit(0);
});

process.stdin.on('end', () => {
  socket.end();
});

process.stdin.on('error', () => {
  socket.end();
});
