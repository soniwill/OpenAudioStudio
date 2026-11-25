# Decisão 002 — Padrão Master Bank / Init Bank para Mixer e Barramentos (FMOD & Wwise)

**Aprovada pelo usuário em 2026-11-12.**

## Contexto e Motivação

O OpenAudioEngine anteriormente explorava um modelo de barramentos modulares e cooperativos onde múltiplos bancos podiam trazer definições de barramentos idênticos, negociando o compartilhamento por contagem de referências fraca (`BankBus` / `std::weak_ptr`) e fazendo comparações profundas (*deep equality*) de parâmetros DSP, posições pré/pós-fader e sidechains, com falha de carregamento (`OAE_ERR_ALREADY_EXISTS`) diante de qualquer divergência.

Esse modelo apresentava riscos de não-determinismo por ordem de carga, acoplamento de ciclo de vida no descarregamento (*unload*) e custo desnecessário de CPU, além de poder falhar o carregamento de bancos em tempo de execução no jogo final.

O usuário determinou abandonar esse mecanismo e adotar estritamente o **padrão consagrado da indústria (FMOD Studio e Audiokinetic Wwise)**.

## Decisões Arquiteturais

1. **Fonte Única da Verdade (*Single Source of Truth*):**
   - Apenas o **Master Bank** (convenção `Master.bank` / projeto que define a chave `buses` com topologia) é proprietário e declara a hierarquia de barramentos do mixer, volumes e cadeias de efeitos DSP no chunk `BUSS`.
   - O Master Bank é carregado no início da aplicação (`oae_studio_load_bank_file("Master.bank")`) para estruturar o mixer do sistema (`oae_system`).

2. **Bancos de Eventos / Conteúdo (*Content Banks*):**
   - Bancos regulares de eventos (ex: `weapons.bank`, `music.bank`) contêm apenas eventos, tracks, instrumentos, lógica, automações, parâmetros e samples.
   - Emitem `bus_count = 0` no chunk `BUSS` (ou não declaram barramentos).
   - Seus eventos apenas referenciam os barramentos de destino pelo caminho (ex: `"bus:/sfx"`).
   - O carregamento de bancos de conteúdo nunca falha por conflito de mixer.

3. **Ciclo de Vida e Descarregamento:**
   - Descarregar um banco de conteúdo (`oae_studio_unload_bank`) encerra e libera apenas as instâncias de eventos e samples daquele banco. O mixer de barramentos e efeitos permanece 100% íntegro e intocado.
   - Descarregar o Master Bank remove os barramentos e efeitos gerenciados por ele.

4. **Tratamento de Roteamento Ausente:**
   - Se um evento apontar para um barramento que não exista no sistema (ex: Master Bank não foi carregado ou caminho inexistente), o motor direciona o áudio para o Master Bus (`bus:/`) padrão com aviso no log de diagnósticos, impedindo falha ou interrupção do gameplay.

5. **Exclusividade do Master Mixer:**
   - Uma vez que o Master Bank instala a árvore de barramentos, nenhum outro banco pode substituir ou conflitar com essa topologia. Tentativas de bancos secundários de sobrescrever a hierarquia de barramentos já instalada são rejeitadas com `OAE_ERR_ALREADY_EXISTS`.
