# T13 — Bank Compiler CLI (`oae_bankc`) Specification & Publication Contract

Este documento especifica o formato de schema JSON de projeto (`project.json` v1), as regras de resolução de caminhos, a validação de referências cruzadas com relatórios de erro formatados por JSON pointer e o protocolo de transação atômica de publicação para o compilador `oae_bankc`.

---

## 1. Linha de Comando da CLI

```text
oae_bankc <project.json> -o <out.bank> [opções]
```

### Argumentos e Flags
* `<project.json>`: Caminho para o arquivo de manifesto de autoria JSON v1.
* `-o <out.bank>`, `--output <out.bank>`: Caminho absoluto ou relativo do arquivo `.bank` de destino gerado.
* `-v`, `--verbose`: Emite detalhes da compilação e mapeamento de chunks na saída padrão.
* `-h`, `--help`: Exibe mensagem de uso e opções disponíveis.

---

## 2. Schema JSON de Projeto (`project.json` v1)

```json
{
  "version": 1,
  "samples": [
    { "id": "laser_sfx", "file": "audio/laser.wav" }
  ],
  "buses": [
    { "path": "bus:/sfx", "volume": 1.0, "effects": [] }
  ],
  "global_parameters": [
    { "name": "WindSpeed", "kind": "continuous", "min": 0.0, "max": 100.0, "default_value": 15.0, "seek_speed": 10.0 }
  ],
  "events": [
    {
      "path": "event:/weapons/laser",
      "kind": "event",
      "volume_db": -3.0,
      "pitch_semitones": 2.0,
      "pan": 0.5,
      "max_instances": 4,
      "steal_mode": "oldest",
      "cooldown_ms": 50,
      "priority": 120,
      "tracks": [
        {
          "volume_db": -1.0,
          "pan": -0.25,
          "instruments": [
            {
              "kind": "single",
              "sample_id": "laser_sfx",
              "offset_ms": 10.0,
              "length_ms": 500.0,
              "loop": false
            }
          ]
        }
      ],
      "logic_items": [
        { "kind": "marker", "name": "hit_point", "position_ms": 250.0 }
      ],
      "automations": [
        {
          "target": "volume",
          "is_timeline": true,
          "curve": {
            "points": [
              { "x": 0.0, "y": 0.0, "shape": "linear" },
              { "x": 500.0, "y": -24.0, "shape": "ease_out" }
            ]
          }
        }
      ],
      "modulators": [
        { "target": "pitch", "kind": "random", "min": -3.0, "max": 3.0 }
      ]
    }
  ],
  "effect_automations": [],
  "vcas": [
    { "path": "vca:/sfx", "buses": ["bus:/sfx"], "events": [] }
  ],
  "mixer": {
    "master": { "volume": 1.0, "mute": false }
  }
}
```

---

## 3. Validações Estritas de Referências Cruzadas

O compilador reporta mensagens legíveis com ponteiro JSON (`JSON path`) em caso de erros:

1. **Arquivo de Sample Inexistente:** Se o arquivo especificado em `/samples/<i>/file` não for encontrado ou não puder ser lido, emite:
   `Error at /samples/<i>/file: File '<path>' not found.`
2. **Bus Inexistente:** Se um evento referenciar um bus inexistente ou VCA referenciar um bus desconhecido:
   `Error at /vcas/<i>/buses/<j>: Bus '<bus_path>' not found.`
3. **Sample Inexistente em Instrumento:** Se um instrumento referenciar um `sample_id` não registrado em `/samples`:
   `Error at /events/<i>/tracks/<t>/instruments/<k>/sample_id: Sample '<id>' not found.`
4. **Ciclo de Eventos Aninhados:** Se um evento de instrumento referenciar um evento gerando dependência cíclica:
   `Error at /events/<i>/tracks/<t>/instruments/<k>/target_event: Nested event cycle detected: '<path1>' -> '<path2>' -> '<path1>'.`
5. **Marcador de Transição Inexistente:** Se um `transition_marker` ou `transition_region` apontar para marcador ausente:
   `Error at /events/<i>/logic_items/<m>/target_marker: Target marker '<name>' not found in event.`

---

## 4. Transação de Publicação Atômica (Seção 4.5)

Para garantir integridade de dados e compatibilidade com consumidores (como Unity):

1. **Staging:** Todos os artefatos são gerados em um diretório temporário `.bankc-stage` no mesmo diretório do arquivo `.bank` de destino.
2. **Cálculo de Catálogo:** O arquivo `.bank.catalog.json` é montado contendo `{ "version": 1, "bank_size": <size>, "bank_crc32": <crc>, "events": [...] }`.
3. **Backup de Catálogo:** Se `<out.bank.catalog.json>` já existir, preserva seu conteúdo para rollback.
4. **Substituição Atômica 1 (Catálogo):** O novo `.bank.catalog.json` substitui atomicamente o existente.
5. **Substituição Atômica 2 (Banco):** O novo `.bank` substitui atomicamente o existente.
6. **Rollback em Caso de Falha:** Se a substituição do `.bank` falhar, restaura o catálogo anterior (ou deleta se não existia) e encerra com código `!= 0`.
7. **Limpeza:** A pasta `.bankc-stage` é removida e o processo encerra com código `0`.

Cada troca usa `MoveFileExW` com `MOVEFILE_REPLACE_EXISTING` no Windows e `rename` nos demais sistemas; o destino não é removido como fallback. A restauração do catálogo anterior também usa troca atômica. As escritas de staging verificam o fechamento do arquivo antes da publicação. Um destino que seja diretório é preservado e gera erro, inclusive quando vazio.

Regressão reproduzível: executar `./scripts/build.ps1 -Config Debug -Full` e repetir com `-Config Release -Full`. O caso `T13: Publication preserves invalid destinations and rolls back catalog` cobre diretórios nos destinos, rollback com e sem catálogo anterior e republicação sobre arquivos existentes.

**Limitação de contrato ainda aberta:** conforme a seção 4.5 do blueprint, concorrência entre compiladores, recuperação após interrupção e falha do próprio rollback ainda exigem decisão. A implementação usa staging compartilhado `.bankc-stage`; não há garantia de publicação concorrente nem recuperação de crash. Atomicidade individual não torna o par banco/catálogo uma transação indivisível.

---

## 5. Evidências de Validação

* Compilado no MSVC x64 19.43 com `/W4 /WX` sem avisos em Debug e Release.
* Executável de linha de comando `native/tools/bankc/oae_bankc.exe` gerado e empacotado.
* Testes E2E executados via `bank_compiler.cpp`:
  * Compilação bem-sucedida de projeto fixture contendo samples, buses, parâmetros globais, eventos multi-faixa, automações (curvas LINEAR e EASE_OUT), marcadores de timeline, snapshots e VCAs.
  * Publicação atômica gerando o banco `.bank` e o catálogo `.bank.catalog.json` correspondente com tamanho e CRC-32 exatos.
  * Carregamento do banco gerado pelo engine via `oae_studio_load_bank_file` e reprodução de áudio verificada acusticamente com pico RMS `> 0.01f`.
  * Verificação e diagnóstico rigoroso com código de erro não-zero para:
    * Arquivo de áudio ausente (`/samples/0/file`).
    * Sample inexistente em instrumento (`/events/0/tracks/0/instruments/0/sample_id`).
    * Detecção e prevenção de ciclo em eventos aninhados (`event:/a` -> `event:/b` -> `event:/a`).
    * Marcador de destino inexistente em transição (`/events/0/logic_items/0/target_marker`).
    * Bus referenciado inexistente (`/events/0/bus_path`).
  * 100% dos testes aprovados no CTest em Debug e Release com `RESULT: OK`.
