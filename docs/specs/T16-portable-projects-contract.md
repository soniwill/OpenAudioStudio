# OpenAudio Studio — Portable Projects, Relinking & Async Bank Publication Contract (T16b / JP02)

## 1. Visão Geral
Este documento especifica o contrato operacional da camada de gerenciamento de projetos portáteis e publicação assíncrona de bancos da DAW desktop **OpenAudio Studio (JUCE 8.0.12 / C++17)**.

---

## 2. Componentes e Arquitetura

### 2.1 Modelo de Documento Portátil (`ProjectDocument`)
- **Arquivo Raiz:** `project.json` v1 em conformidade estrita com o schema especificado no Master Blueprint (Seção 4.4).
- **Estrutura de Pastas de Projeto:**
  ```text
  <ProjectRoot>/
    project.json
    assets/           # Áudios WAV/FLAC/OGG copiados e referenciados por caminhos relativos
    build/            # Snapshots, bancos compilados (.bank) e catálogos (.bank.catalog.json)
  ```
- **I/O Atômico e Resiliente:**
  - Salvamentos utilizam escrita em `<project.json>.tmp`, seguida de substituição transacional nativa (`ReplaceFileW` / `MoveFileExW` no Windows).
  - Em caso de falha de permissão, disco cheio ou erro de formatação, o arquivo original existente é 100% preservado sem corrupção.
- **Portabilidade Total:**
  - Todas as referências a assets de áudio são mantidas como caminhos relativos (`assets/<file>`).
  - A pasta inteira do projeto pode ser movida ou renomeada (inclusive em caminhos contendo espaços, acentuação e caracteres Unicode/UTF-8) sem perder a resolução de assets.
- **Detecção e Relink de Assets:**
  - `check_missing_assets()` identifica qualquer arquivo ausente em tempo de execução.
  - `relink_asset()` permite remapear e copiar novos arquivos para `assets/`.

### 2.2 Publicador Assíncrono de Bancos (`AsyncBankPublisher`)
- **Snapshot Imutável:**
  - A compilação opera sobre uma cópia imutável (deep snapshot JSON) capturada instantaneamente na thread principal (`create_immutable_snapshot()`), evitando condições de corrida com edições posteriores do usuário.
- **Execução em Segundo Plano:**
  - Disparo de thread dedicada (`std::thread`) invocando o compilador nativo `oae_bankc.exe` através de pipes anônimos (`CreateProcessW`) para captura completa de stdout/stderr.
- **Isolamento de Falhas e Preservação:**
  - O compilador recebe diretamente o caminho final solicitado. O staging e a publicação catálogo-primeiro/banco-depois ficam exclusivamente sob o protocolo T13; o Studio não copia nem remove os artefatos publicados.
  - A gravação e o fechamento do snapshot são verificados antes de iniciar o compilador. Falhas de gravação ou do processo retornam `success = false` e código não zero nos diagnósticos.
  - Se a compilação falhar (erro de sintaxe, referência a asset ausente ou ciclo), o banco válido anteriormente publicado é preservado intacto.
  - Mensagens de erro com JSON path e diagnósticos são disponibilizadas para o console de logs thread-safe.

---

## 3. Validação Automatizada (`--smoke-project`)

O runner `--smoke-project` executa sem intervenção humana:
1. **Ambiente com Caracteres Especiais:** Criação do projeto em pasta temporária contendo espaços e Unicode (`OAE Projét Áudio 🎵 Spaced`).
2. **Importação:** Geração de tom senoidal sintético externo (WAV 440 Hz, 48 kHz, 16-bit PCM), importação com cópia para `assets/` e normalização de caminho relativo.
3. **Persistência Atômica:** Adição de evento e salvamento atômico de `project.json`.
4. **Publicação Assíncrona:** Compilação em thread de background gerando `.bank` e `.bank.catalog.json`.
5. **Auditoria Nativa:** Carregamento do banco via `StudioEngine` (modo headless), instanciação do evento, renderização em buffer de áudio e validação de amplitude de pico (`peak > 0.001f`).
6. **Relocation (Portabilidade):** Movimentação da pasta inteira do projeto para outro local com caracteres Unicode (`OAE Projét Movido 🚀 Destino`), abertura do projeto, verificação de zero missing assets e recompilação bem-sucedida.
7. **Robustez a Falhas:**
   - Teste de falha de save intencional validando que o `project.json` anterior não é corrompido.
   - Teste de falha de build intencional (referência inválida) validando captura de mensagem de erro e preservação do arquivo `.bank` existente.
   - Destino de catálogo bloqueado por diretório vazio: publicação retorna falha e preserva byte a byte o banco anterior, sem remover o diretório bloqueador.
   - Publicação diretamente em `build/published.bank`, seguida de compilação inválida: banco e catálogo anteriores permanecem idênticos byte a byte.

Execute `./scripts/build_studio.ps1 -Config Debug` e `./scripts/build_studio.ps1 -Config Release` para compilar e executar os smokes, incluindo essas regressões. O resultado esperado é `RESULT: OK`. A política de concorrência entre publicadores, recuperação de crash e falha do próprio rollback permanece pendente conforme a seção 4.5 do blueprint; este contrato não promete uma troca indivisível do par banco/catálogo.
