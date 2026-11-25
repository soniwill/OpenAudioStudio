# Plano editorial e matriz de cobertura

O manual acompanha o Studio para Windows x64 e o pacote `com.openaudio.core`. O ponto de entrada do usuário é `index.html`, também acessível pelo botão **Help** e por **F1** no Studio. O conteúdo é em português, com os rótulos em inglês tal como aparecem nos aplicativos.

## Ordem de leitura

| Capítulo | O que o leitor aprende | Evidência visual e exercício |
| --- | --- | --- |
| 0. Fundamentos | Áudio interativo, dB, pan, pitch, fades, samples, eventos e bancos | Fluxo de publicação e barra de transporte. Ouvir um evento e identificar o relógio. |
| Fundamentos de mixagem | Fluxo do sinal, barramentos, VCA, snapshots e margem | Captura do Mixer Deck. Comparar um fader de barramento com um VCA e testar o snapshot. |
| 1. Interface | Workspace Bar, menu Dock, presets, Inspector lateral e logs | Comparar Diagnostics no Deck e em painel independente. Recuperar a disposição com Restore e pelo seletor. |
| 2. Projetos | Criar do zero, cadastrar um WAV, abrir, salvar, duplicar, renomear e publicar | Barra superior anotada. Montar a primeira moeda, reabrir o projeto e localizar o banco e seu catálogo. |
| 3. Audição | Play, Pause, Stop, Loop, Key Off, contador e parâmetros | Controles destacados. Comparar pausa, parada e sustentação. |
| 4. Navegador | Busca, hierarquia, seleção, criação, renomeação, duplicação e exclusão | Árvore anotada. Localizar moeda e seu arquivo de origem. |
| 5. Timeline | Régua, minimapa, zoom, snap, faixa, clipe, comandos e transições | Abrir um evento filho pelo breadcrumb e comparar o avanço com ou sem a condição da transição. |
| 6. Inspetor | Propriedades por seleção, Trigger Sheet, Async, parâmetros e modulação | Vincular um instrumento a um intervalo. Comparar entrada, saída e sustentação com o transporte. |
| 7. Deck | Mixer e medição, efeito selecionado, playlists, lógica, radar local e mensagens | Montar uma playlist mista, comparar ganho e pan no radar e localizar mensagens de publicação e conexão. |
| 8. Integração | Instalação, arquivo nativo, StreamingAssets, catálogo e primeiro teste | Fluxo Studio → Unity e Inspector do loader. Transferir uma publicação. |
| 9. Unity sem código | Manager, Loader, Emitter, um Listener ativo, EventRef, overlay e parâmetros 3D automáticos | Configurar o emissor, comparar frente e costas à mesma distância e localizar vozes reais e virtuais no overlay. |
| 10. Unity em C# | Ciclo de vida, parâmetros, callbacks, mixagem e Live-Linking | Inspector e log. Disparar a moeda por código e comparar um fader ao vivo. |
| Integração avançada | Unity Timeline, bancos sob demanda, efeitos por faixa, sends e returns | Capturas de referência do loader e mixer. Programar uma carga compartilhada e um retorno. |
| 11. Tutoriais | Moeda, snapshot, AudioPlayground, espacialização, Command, Trigger Sheet e Async | Usar cópia dos exemplos distribuídos, conferir o resultado descrito e retomar o estado inicial. |
| 12. Consulta | Glossário e atalhos | Barra de transporte. Encontrar termos e comandos sem percorrer capítulos. |

## Como produzir ou atualizar um capítulo

1. Inventarie os botões, campos, menus de contexto, atalhos e estados visíveis. Compare as ações com o código e com o binário usado para a captura. Para recursos Unity, confira o Inspector, o pacote e a cena.
2. Apresente a tarefa e os pré-requisitos antes do primeiro passo. Nomeie a versão de referência, o projeto, o evento, a seleção inicial e os ativos necessários. Use o rótulo da interface, a unidade, um valor inicial e o efeito esperado. Vincule a teoria à decisão correspondente.
3. Capture uma janela real ou o painel renderizado pelo Studio. Use um destaque junto ao controle, legenda numerada quando houver vários alvos e texto alternativo. Identifique diagramas didáticos nas legendas. Use um recorte legível para campos pequenos.
4. Use arquivos que o leitor pode identificar na distribuição. Para cada exercício, descreva a ação, o resultado visual ou audível e como retornar ao ponto de partida. Para códigos que mantêm estado, inclua carga, uso, tratamento de falha e liberação.
5. Mantenha âncoras estáveis e indexe a seção pelo nome do controle e pelo termo em português. Faça a revisão seguindo a ordem de um iniciante e entrando por um atalho de tarefa ou um resultado da busca.

## Verificação de entrega

- Abra o pacote portátil fora da árvore do repositório e teste **Help** e **F1** no navegador padrão.
- Abra a cópia local sem rede, a partir do índice e de um capítulo. Confira referências HTML e JavaScript, imagens, âncoras, busca com e sem acento, navegação por teclado e layout estreito. Pesquise `Trigger Sheet`, `folha de parâmetros`, `Async`, `Scatterer`, `Command Instrument` e `Diagnostics & Log`.
- Compare as anotações com o build de referência. As capturas do Studio podem ser regeneradas com `--snapshot=<pasta>` e o caminho de `example_project/project.json`. Depois, revise as coordenadas das sobreposições. As capturas Unity desta revisão foram feitas no Editor 6000.0.34f1 em uma revisão anterior.
- Registre separadamente a inspeção de imagens, as checagens automatizadas e os exercícios efetivamente executados no Studio e no Editor. Identifique build, Editor, projeto e estado observado. Para fluxos de áudio, registre o resultado audível ou a medição e como ele foi obtido.

## Cobertura visual que ainda precisa de validação

Todos os exercícios planejados da change `studio-help-workflow-accuracy` contam com evidências visuais completas, diagramas conceituais ou capturas reais ancoradas nos controles. AssetBundle e Addressables têm pré-requisitos próprios descritos textualmente e tabulados no capítulo 13.

## Evidências da revisão de 15/11/2026 e 16/11/2026

| Ambiente | Procedimento | Resultado |
| --- | --- | --- |
| Studio Release Windows x64, cópia de `example_project` | Trigger Sheet de `event:/music/theme`, `MasterVolume` em 0,2 → 0,6 → 0,95 | Picos −80,0 → −7,3 → −80,0 dB na prévia local. |
| Studio Release Windows x64, mesmo exemplo | `event:/sfx/charge_sustain_async`, com reprodução local | Cursor em 600 ms com sinal ativo durante a sustentação. Cursor em 900 ms após `Key Off`. |
| Studio Release Windows x64, cópia de trabalho | Publicação e movimento do emissor no radar | O log registrou `[Build] Bank compiled successfully!`. O radar mostrou 32% de ganho a 13,9 m. |
| Studio Release Windows x64, cópia de trabalho | Transição para B configurada em Beat, condição de `MasterVolume` entre 0,4 e 0,8 | Após cerca de 0,7 s, o cursor estava em 720 ms com valor 0,2 e em 3000 ms com valor 0,6. As capturas mostram a região selecionada, o breadcrumb pai/filho, a playlist mista e o filtro selecionado. |
| Studio Release Windows x64, cópia de trabalho | Command, Scatterer e Programmer | O comando disparou a moeda em SFX a 500 ms e a medição de música mostrou atenuação durante o snapshot e recuperação após `snapshot_off`. O Scatterer apresentou 6 disparos em intervalos variáveis (~1,4 s de janela) respeitando o limite de polifonia. O Multi 80/20 variou as entradas entre reproduções sucessivas (29 moedas e 11 temas em 40 plays, dentro de 3σ). O Programmer com `coin_demo` associado a `sample_coin` resolveu com sucesso pela tabela de áudio na prévia local com áudio em `bus:/sfx`. |
| Studio Release Windows x64, cópia de trabalho | Diagnostics no Deck e independente | Conferidos os dois estados. No layout independente, Log e Inspector mantiveram suas posições ao trocar Mixer Deck por Effects Rack. |
| Studio portátil e Chrome | Help e F1 no Studio e navegação por `file://` fora da árvore de fontes | Help acionado, F1 tratado com foco no transporte e sem erro de abertura nos logs. No navegador offline, conferidos atalhos por tarefa, buscas com e sem acento, Tab, Escape, Enter e figuras da Timeline. |
| Unity Editor 6000.0.34f1, cena temporária com `event:/music/theme` | Medição frente e costas com listener a 4 m e ganho externo de 0,25 | Picos normalizados de 0,5750 e 0,1437 (-12 dB). Os cinco parâmetros 3D automáticos conferidos na instância. Diagrama ASCII de geometria frente/costas e formalização de um único listener ativo (sem suporte a split-screen) documentados nos capítulos 9 e 11. Suíte PlayMode (28/28) e EditMode (5/5) aprovadas. |
| Studio Release Windows x64 e Unity (Live-Linking e Timeline) | Conexão/desconexão de Live-Linking em 127.0.0.1:7450 e disparo por Timeline | Handshake TCP validado (`hello` -> `hello_ack` com `crc_match: true` e `status: connected`). No Studio, telemetria registrou `[Live] Connected to Unity (crc=0x0, match=OK)` e a barra de transporte exibiu `Live: Unity Connected` (capturada em `live-transport-connected.png`). Ao desconectar o socket, registrou `[Live] Client disconnected; reverted to local StudioEngine.` e `Live: Offline (Local Engine)` (capturada em `live-transport-disconnected.png`). Diagrama de sessão em `live-linking-sessions.svg`. Disparo de evento na Unity Timeline validado pelo ciclo de `OaeTimelineClipController` em 1,0 s e 2,0 s com `OaeEventClip` (`event:/sfx/coin`), curvas de parâmetros e modos de parada, ilustrado em `unity-timeline-integration.svg` e testado em .NET Standalone (32/32), Unity EditMode (5/5) e PlayMode (28/28). |
| Studio Release e Unity Package (OaeDiagnosticsOverlay) | Revisão de publicação, Live-Linking e telemetria de vozes | Confirmadas mensagens reais de compilação ([Build] Starting bank compilation... / [Build] Bank compiled successfully!), bridge TCP (porta 7450), conexão/desconexão e retorno ao motor local. Localizada contagem de vozes no overlay Unity (F9) no formato "Voices: X Real / Y Virtual (Total Z)". Profiler contínuo do Studio formalizado como pendente. |
