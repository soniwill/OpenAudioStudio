# Épico 8 — Conectividade em Tempo Real / Live-Linking (`LIVE-01` a `LIVE-04`)

## Objetivo e Escopo

Estabelecer uma ponte TCP de baixa latência (`< 10 ms` em loopback) entre o **OpenAudio Studio** (servidor na porta `7450`) e o runtime **OpenAudio Unity** (`LiveClient.cs`), permitindo:
- **LIVE-01:** Handshake de versão e validação de `bank_crc32`.
- **LIVE-02:** Conexão automática não-bloqueante no cliente Unity (`PlayMode` / `Development Builds`).
- **LIVE-03:** Sincronização bidirecional de faders, parâmetros e curvas entre Studio e jogo.
- **LIVE-04:** Auditoria remota no `AudioListener` do jogo conectado, com fallback automático para a engine local do Studio quando desconectado.

### Não Escopo
- Criptografia TLS ou autenticação remota fora de `127.0.0.1` / LAN confiável (ferramenta de desenvolvimento local).
- Hot-reload binário de arquivos `.bank` inteiros pela rede (fora do escopo de Live-Linking; exige republicação).

---

## Protocolo de Fio (`TCP :7450`)

- **Transporte:** TCP `127.0.0.1:7450` (configurável para testes), `TCP_NODELAY` habilitado.
- **Enquadramento:** Mensagens JSON UTF-8 delimitadas por `\n` (*newline-delimited JSON*).
- **Versão do Protocolo:** `1`.

### 1. Handshake (`LIVE-01`)

1. **Cliente -> Studio (`hello`):**
```json
{"type":"hello","version":1,"client":"Unity","bank_crc32":287454020}
```
2. **Studio -> Cliente (`hello_ack`):**
```json
{"type":"hello_ack","version":1,"studio_version":"1.0.0","studio_bank_crc32":287454020,"crc_match":true,"status":"connected"}
```
- **Invariante de `bank_crc32`:** Se ambos `studio_bank_crc32 != 0` e `bank_crc32 != 0` e forem diferentes, `crc_match` é `false` e o Studio registra diagnóstico de divergência de banco sem derrubar o socket. Se um deles for `0` (ex.: nenhum banco carregado/publicado), `crc_match` é `true`.
- **Versão incompatível:** Se `version != 1`, Studio responde `{"type":"hello_ack","version":1,"status":"version_mismatch","crc_match":false}` e fecha a conexão.

### 2. Sincronização Bidirecional (`LIVE-03`)

#### Studio -> Jogo
- `{"type":"bus_volume","path":"bus:/SFX","volume_db":-6.0}`
- `{"type":"bus_pan","path":"bus:/SFX","pan":-0.25}`
- `{"type":"bus_mute","path":"bus:/SFX","mute":true}`
- `{"type":"vca_volume","path":"vca:/Music","volume_db":-3.0}`
- `{"type":"param_set","name":"RPM","value":0.75,"is_global":true,"event_path":""}`
- `{"type":"curve_update","event_path":"event:/Engine","track_index":0,"target":"volume","points":[{"x":0.0,"y":0.0},{"x":1.0,"y":-6.0}]}`
- `{"type":"ping","ts_ms":1731450000000}` -> Jogo responde `{"type":"pong","ts_ms":1731450000000}`

#### Jogo -> Studio
- `{"type":"param_set","name":"RPM","value":0.82,"is_global":true,"event_path":""}`
  - O Studio aplica o valor recebido no `AuditionTransport` **sem retransmitir** para o jogo (prevenção de loop de eco) e atualiza os cursores na UI (`TransportBar`).

### 3. Auditoria Remota & Fallback Offline (`LIVE-04`)

#### Studio -> Jogo
- `{"type":"event_play","event_path":"event:/Explosion","position_ms":0.0,"loop":false}`
- `{"type":"event_stop","event_path":"event:/Explosion","immediate":false}`
- `{"type":"event_key_off","event_path":"event:/Explosion"}`
- `{"type":"snapshot_toggle","snapshot_path":"snapshot:/DuckMusic","active":true}`

#### Comportamento de Roteamento & Fallback
- Quando `LiveBridgeServer::is_client_connected()` é `true` e o modo remoto está habilitado (padrão quando conectado), `AuditionTransport` envia os comandos de audição para o jogo conectado (que instancia e reproduz no `OaeSystem` do jogo, acoplado ao `AudioListener`), em vez de tocar duplicado na engine local do Studio.
- Se a conexão TCP cair ou o cliente desconectar, `LiveBridgeServer` limpa o estado de conexão e `AuditionTransport` volta automaticamente ao modo offline, executando novas audições diretamente na `StudioEngine` local.

---

## Threading e Segurança

- **Studio (`LiveBridgeServer`):**
  - Threads dedicadas de *accept* e leitura TCP; envio protegido por mutex com `TCP_NODELAY`.
  - Atualizações recebidas do jogo são enfileiradas em fila protegida por mutex e drenadas na JUCE Message Thread (`poll_incoming()` / `timerCallback()`), nunca tocando componentes JUCE diretamente da thread de rede.
- **Unity (`LiveClient`):**
  - Conexão e leitura rodam em thread de background (não-bloqueante para `Update()`).
  - Comandos recebidos são enfileirados em `ConcurrentQueue` e aplicados na thread principal dentro de `LiveClient.Update()` (chamado por `OaeAudioManager.Update()`).
  - Em `UNITY_EDITOR || DEVELOPMENT_BUILD`, `OaeAudioManager` inicia `LiveClient` automaticamente se `m_EnableLiveLinking == true`.

---

## Critérios de Aceitação

1. **LIVE-01:** `LiveBridgeServer` escuta na porta `7450`, aceita handshake `hello`/`hello_ack`, valida versão e detecta `crc_match == true` vs `false` quando `bank_crc32` diverge.
2. **LIVE-02:** `LiveClient.cs` conecta de forma não-bloqueante ao Studio em PlayMode / Development Builds / testes .NET, completa o handshake e tolera servidor ausente sem travar ou lançar exceção não tratada.
3. **LIVE-03:** Alterações de fader (`bus_volume`), `pan`, `mute`, `vca_volume` e `param_set` no Studio são refletidas no `OaeSystem` do cliente em `< 10 ms` (loopback); alterações de parâmetros no cliente atualizam o `AuditionTransport` no Studio em `< 10 ms` sem loop de eco.
4. **LIVE-04:** Com cliente conectado, `AuditionTransport::play()` / `stop()` dispara e para o evento no `OaeSystem` remoto do cliente; após desconexão do cliente, `AuditionTransport::play()` volta automaticamente a tocar na `StudioEngine` local.
5. **Validação:** `--smoke-live` aprovado em `scripts/build_studio.ps1` (Debug e Release) e testes C# em `LiveClientTests.cs` aprovados em `scripts/unity_test.ps1 -DotnetOnly` (e CTest nativo intacto).
