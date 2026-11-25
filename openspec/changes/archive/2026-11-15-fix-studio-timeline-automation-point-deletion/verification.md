# Verification — fix-studio-timeline-automation-point-deletion

## Gates (executados em 2026-11-15 pelo subagente `oas-verificacao`)

| Comando | Resultado |
| --- | --- |
| `./scripts/verify.ps1 -Layer studio` | exit 0 — `ALL SMOKE TESTS PASSED (Config: Release) - RESULT: OK`, 11/11 smokes |
| `./scripts/verify.ps1 -Layer docs` | exit 0 — `Totals: 10 passed, 0 failed (10 items)` + `RESULT: OK` |
| `openspec validate fix-studio-timeline-automation-point-deletion --type change --strict` | exit 0 — `Change 'fix-studio-timeline-automation-point-deletion' is valid` |

Evidência do escopo no `--smoke-timeline`: `[OAE Studio] Point deletion, last point curve removal and Delete routing verified.`

Falha transitória na primeira execução do gate Studio: `--smoke-pro-ui` falhou por causa de `oae_studio_layout.json` residual (`"timeline": {"visible": false}`) de execução anterior interrompida; com o layout residual removido, o gate completo passou duas vezes do zero (exit 0). Não é regressão da change; achado registrado como candidato a change futura (o smoke deveria normalizar/ignorar layout persistido).

Logs em `%TEMP%\opencode\oae-gates-2026-11-15\` (`studio3.log`, `studio2.out.log`, `pro-ui-rerun.log`, `layout.before.json`, `docs2.log`, `openspec-validate.log`).

## Cenário MCP com prévia ativa (Studio Release, `--mcp`, porta 7451)

Sequência executada ao vivo na janela do Studio com o evento `theme` aberto, pista 1 e faixa `Volume Automation` expandida; curva temporal com dois pontos (1360 ms e 3360 ms, −35 dB). Ferramentas: `studio_ui_mouse`, `studio_ui_key`, `studio_ui_inspect`, `studio_ui_screenshot`, `studio_transport`. Cada resposta está em `evidence/step-NN-*.json` (`isError: false` em todas).

1. **Seleção e exclusão por tecla** — clique no ponto de 1360 ms (`step-01`): a faixa capturou foco (`focused: true`, `wants_focus: true`). `delete` (`step-02`): tecla consumida e o diff removeu apenas o ponto selecionado — `[{replace /points/0/x → 3360}, {remove /points/1}]` — preservando as demais automações da pista.
2. **Undo/Redo em passo único** — `ctrl+z` (`step-03`) restaurou o ponto (curva de volta com 1360+3360) e `ctrl+y` (`step-04`) excluiu novamente, cada um com um único passo no histórico.
3. **Último ponto** — clique no último ponto restante (`step-06`) e `delete` (`step-07`): diff `[{remove /events/0/tracks/0/automations/0}]` — a entrada `target=volume, is_timeline=true` foi removida inteira, sem tocar nas demais automações. `studio_ui_inspect` de `track_lane_0` (`step-08`) confirmou a faixa ainda presente (518×60 em y=286) e vazia, sem curva.
4. **Áudio sem erro com a curva removida** — `studio_transport play` (`step-10`) e inspeção durante a reprodução (`step-11`): timecode avançando (`00:01.460`), status `Preview In Sync (Rev 6)`, sem erro. `stop` em `step-12`.

Screenshots: `evidence/01-lane-expanded.png`, `evidence/01-two-points-curve.png`, `evidence/02-one-point-left.png`, `evidence/03-empty-grid.png`.

A conexão MCP usou o servidor TCP do próprio executável (`--mcp`) por uma sonda JSON-RPC local (`%TEMP%\opencode\oas-mcp-evidence\mcp_probe.js`), equivalente à ponte `scripts/mcp_bridge.js` registrada no `opencode.json`. As ferramentas `studio_ui_*` carregaram nesse modo; na sessão do orquestrador o servidor `openaudio-studio` não estava no catálogo no início da sessão (Studio fechado), por isso a sonda direta.

## Manual offline (tarefa 2.4)

O capítulo 5 (`docs/manual/chapters/05-timeline-e-waveforms.html`, seção 7, "Automação de volume no tempo") já documentava o gesto de exclusão por `Delete`/`Backspace`, a opção **Delete Point** do menu de contexto (rótulo confere com `studio/Source/UI/TimelineTrackLane.cpp:879`), o comportamento do último ponto (curva removida, grade vazia), o roteamento da tecla pelo último alvo clicado e o Undo/Redo em passo único. Âncoras e navegação verificadas; nenhum uso de `Delete` para editar ponto fora da faixa focada.