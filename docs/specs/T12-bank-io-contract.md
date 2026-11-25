# T12 — Bank Format Writer/Reader, Fuzzing & Lifecycle Contract

Este documento especifica os detalhes do formato binário de bancos (`.bank`), os componentes de serialização (`bank_writer`) e deserialização (`bank_reader`), fuzzing de robustez e gerenciamento de ciclo de vida de bancos no OpenAudioEngine.

---

## 1. Especificação do Formato Binário (`.bank`)

### 1.1 Cabeçalho
* **Magic:** `OAEB` (`0x4245414F` little-endian).
* **Version:** `1` (u16).
* **Flags:** `0` (u16).
* **Chunk Count:** `u32`.
* **Directory Table:** Entradas de 24 bytes por chunk:
  * `tag`: `char[4]`
  * `offset`: `u64` (absoluto, múltiplo de 16, `>= 12 + chunk_count * 24`)
  * `length`: `u64`
  * `crc32`: `u32` (calculado sobre o payload com algoritmo ISO-HDLC / IEEE 802.3)

### 1.2 Regras Estritas de Validação
1. **Verificação de Chunks Obrigatórios:** Os 5 chunks core (`STRS`, `BUSS`, `EVTS`, `SMPL`, `DATA`) são obrigatórios. Chunks de extensão (`LOGC`, `PARM`, `SNAP`, `VCAS`) são opcionais na leitura.
2. **Offsets e Limites:** Nenhum chunk pode ter offset não alinhado em 16 bytes, exceder o tamanho do buffer ou sobrepor-se a outro.
3. **CRC32:** O CRC32 de todo chunk deve coincidir antes do início da decodificação. Qualquer divergência retorna `OAE_ERR_FORMAT`.
4. **Booleans:** `u8` estritamente restrito a `0` ou `1`. Qualquer valor `> 1` retorna `OAE_ERR_FORMAT`.
5. **Strings:** Codificadas em UTF-8 válido, sem bytes `0x00` embutidos. Índices fora da tabela `STRS` retornam `OAE_ERR_FORMAT`.
6. **Consumo Completo:** Ao terminar de ler qualquer chunk conhecido (exceto `DATA`), `remaining_bytes` deve ser exatamente `0`.

---

## 2. API C de Bancos

O campo wire `range` de moduladores RANDOM restaura o intervalo simétrico
`[-range, +range]` produzido pelas factories `random_volume`, `random_pitch` e
`random_pan`. Exemplo: `random_pitch(3)` mantém `[-3, +3]` após write/read.
O teste de round-trip cobre os dois extremos nas três propriedades. Intervalos
assimétricos configurados diretamente no modelo não são representáveis por esse
único campo; sua política de rejeição/compatibilidade está pendente na proposta
`bank-v1-clarifications-proposal.md`.

* `oae_studio_load_bank_memory(oae_system system, const void* bytes, size_t size, oae_bank* out_bank)`
* `oae_studio_load_bank_file(oae_system system, const char* path, oae_bank* out_bank)`
* `oae_studio_unload_bank(oae_system system, oae_bank bank)`

---

## 3. Critérios de Aceitação (AC)

1. **Round-Trip Exato:** Serializar um `ModelSet` completo e deserializá-lo reproduz todos os campos com fidelidade exata.
2. **Fuzzing Robusto (10.000 iterações):** Mutações aleatórias de bytes no banco serializado nunca causam crash ou vazamento; retornam `OAE_ERR_FORMAT`.
3. **Descarregamento Seguro:** `oae_studio_unload_bank` para todas as instâncias em execução oriundas do banco, remove os modelos e invalida o handle `oae_bank`.
4. **Gates:** Build Debug e Release com zero avisos (`/W4 /WX`) e 100% dos testes aprovados.

---

## 4. Evidências de Validação

* Compilado no MSVC x64 19.43 com `/W4 /WX` sem avisos em Debug e Release.
* 2/2 testes CTest (`native` e `c_api_smoke`) aprovados com `RESULT: OK`. Total de 66 casos doctest e 63.334 asserções aprovados sem falhas.
* Round-trip completo serializou e recuperou com fidelidade exata parâmetros globais, eventos multi-faixa, instrumentos, automações de volume (curvas com LINEAR e EASE_OUT), modulação de pitch, marcadores de timeline, snapshots e VCAs.
* 10.000 iterações de fuzzing com `Pcg32` (bit flips, sobreposições de inteiros, truncamento e trailing garbage) executadas sem nenhum crash ou leak de memória, com detecção e rejeição rigorosa via `OAE_ERR_FORMAT`.
* Ciclo de vida de carregamento de banco em memória e arquivo, reprodução simultânea de instâncias e parada imediata com invalidação de handles sob `oae_studio_unload_bank` 100% verificado.
