# OpenAudioStudio

O OpenAudioEngine é o motor de áudio interativo em C++17 com ABI C. O OpenAudio Studio é a ferramenta de autoria, audição e publicação de bancos em JUCE. O pacote Unity integra os bancos ao jogo e inclui a cena de exemplo `AudioPlayground`, uma demo de áudio 3D em primeira pessoa. O [servidor MCP local do Studio](docs/specs/mcp-server-guide.md) oferece automação de autoria e inspeção da interface.

Abra [o manual do Studio e da Unity](docs/manual/index.html) para seguir um exemplo de criação de projeto, publicação de banco e uso no jogo. Para trabalhar no repositório, consulte [AGENTS.md](AGENTS.md), [o índice de capacidades](docs/specs/README.md) e [o blueprint consolidado](docs/legacy/blueprint.md). Registre mudanças em `openspec/changes/`. Os requisitos migrados vivem em `openspec/specs/`.

Após clonar o repositório, inicialize o JUCE fixado para o Studio.

```powershell
git submodule update --init --recursive
```

## Compilar e testar

Use CMake 3.25 ou posterior, PowerShell e uma toolchain que ofereça C11 e C++17. No Windows, instale o Visual Studio 2022 Build Tools com a carga de trabalho de C++ para desktop e um Windows SDK. O script procura CMake no PATH e, no Windows, também na instalação do Visual Studio. CMake e CTest devem vir da mesma instalação.

```powershell
./scripts/build.ps1 -Config Debug -Full
./scripts/build.ps1 -Config Release -Full
```

Cada comando gera as bibliotecas estática e compartilhada em `build/<Config>` e executa os testes nativos. Os testes cobrem decodificação de WAV, fila SPSC e a API de sistema em modo headless. Com sucesso, o script termina com `RESULT: OK`. Uma falha produz `RESULT: FAILED` e código de saída 1.

| Opção | Comportamento e exemplo |
| --- | --- |
| `-Config` | Seleciona Debug (padrão) ou Release. Exemplo `./scripts/build.ps1 -Config Release`. |
| `-Full` | Limpa a configuração selecionada, compila e executa todos os testes. |
| `-Clean` | Limpa a configuração selecionada antes da compilação. Exemplo `./scripts/build.ps1 -Config Debug -Clean`. |
| `-TestFilter` | Filtra nomes de testes CTest por expressão regular. Exemplo `./scripts/build.ps1 -TestFilter '^native$'`. Sem correspondência, falha. |
| `-NoTests` | Gera as bibliotecas sem executar testes. Exemplo `./scripts/build.ps1 -Config Release -NoTests`. |

O script reaproveita o diretório da configuração por padrão. Use `-Clean` se trocar o gerador ou a toolchain. Se CMake não encontrar o compilador no Windows, verifique a instalação da carga de trabalho e do SDK. Se usar Ninja no Windows, abra um terminal com o ambiente MSVC configurado.

Veja as [revisões, licenças e hashes das dependências](native/external/README.md).

Para compilar o Studio e executar os smoke tests, use `./scripts/build_studio.ps1 -Config Release`. O bundle sai em `build/Studio_Bundle_Release/`. Os testes de Live-Linking exigem loopback local. Para auditar e verificar todos os controles interativos da UI do Studio em modo headless, execute `./scripts/test_studio_controls.ps1` (consulte o [guia de verificação de controles da UI do Studio](docs/testing/studio-ui-controls.md)).

Para preparar as demos da Unity em um clone novo no Windows, execute na raiz:

```powershell
./scripts/build.ps1 -Config Release -Full
./scripts/create_playground_assets.ps1 -SyntheticOnly
./scripts/deploy_unity.ps1 -Config Release
./scripts/unity_test.ps1 -TestPlatform Both
```

Os scripts precisam do compilador de bancos `oae_bankc.exe`, gerado pelo build nativo, e preparam os bancos e catálogos em `StreamingAssets`. O modo `-Offline` usa as alternativas locais do gerador de áudio. Use o SDK .NET e um Unity Editor compatível com [a versão do projeto](unity/OpenAudioUnity/ProjectSettings/ProjectVersion.txt). Use `-DotnetOnly` no script de testes para executar apenas a suíte .NET.

## Verificar uma mudança

Com o Git e o CLI do OpenSpec disponíveis, execute na raiz do repositório.

```powershell
./scripts/verify.ps1 -Layer docs
./scripts/verify.ps1 -Layer native,studio
./scripts/verify.ps1
./scripts/verify.ps1 -All
```

Sem parâmetros, o script identifica as camadas alteradas pelo Git. `-All` executa os gates de documentação, motor, Studio e Unity e é obrigatório antes do arquivamento de uma change. O script retorna `RESULT: OK` (0) em sucesso, `RESULT: FAILED` (1) em falha ou `RESULT: PENDING/BLOCKED` (2) quando um pré-requisito obrigatório do gate completo está indisponível. Uma dispensa de gate para arquivamento exige autorização explícita.

Para criar e renderizar um sistema sem dispositivo, veja a [referência da API C](docs/specs/system-api.md). A [especificação da fila SPSC](docs/specs/spsc-queue.md) inclui seu contrato de concorrência e exemplo.

Para carregar, reproduzir e liberar áudio, consulte o [contrato de sons e canais](docs/specs/T03-sound-contract.md).

Para organizar canais em grupos de mixagem, consulte o [contrato de buses](docs/specs/T04-bus-contract.md).

Para configurar prioridades e o teto de canais processados, consulte o [contrato de vozes](docs/specs/T05-voice-contract.md).

Para pan, fades e reprodução em frames precisos, consulte o [contrato de agendamento](docs/specs/T06-timing-contract.md).

Para inserir efeitos, configurar sidechain e ler Peak/RMS, consulte o [contrato de efeitos e medição](docs/specs/T07-effects-contract.md).

## Release portátil para Windows

Em **Actions → Release Windows → Run workflow**, escolha `main` e informe uma versão nova, por exemplo `v0.1.1`. A execução padrão gera o Studio portátil, as fontes correspondentes, a integração Unity com a DLL x64 e `SHA256SUMS.txt`. O Studio inclui os projetos de autoria do AudioPlayground, os WAVs aprovados e os dois bancos compilados.

Para também gerar o **AudioPlayground jogável**, habilite `unity_player`. Esse job precisa de um runner Windows x64 dedicado, com Unity 6000.0.34f1 ativado, Windows Build Support, PowerShell 7 e a etiqueta `oae-unity`. Em **Settings → Secrets and variables → Actions → Variables**, defina `OAE_UNITY_RUNNER_ENABLED=true` depois de configurar o runner. Use `OAE_UNITY_EDITOR` se o executável estiver fora do caminho padrão do Unity Hub. O runner deve executar somente código confiável desta branch. Veja [como configurar runners no GitHub](https://docs.github.com/en/actions/how-tos/manage-runners/self-hosted-runners/add-runners).

Selecione `create_draft` apenas para o release completo. O workflow exige Studio, fontes, pacote Unity, demo e checksums antes de criar um rascunho novo. Um release ou rascunho com a mesma versão impede a criação. Baixe e teste a demo extraída antes de publicar.

Para gerar os mesmos arquivos localmente, parta de um checkout limpo com JUCE inicializado e faça um build Windows x64:

```powershell
./scripts/build_studio.ps1 -Config Release
./scripts/package_studio_release.ps1 -Version v0.1.1
./scripts/package_unity_release.ps1 -Version v0.1.1 -IntegrationOnly -RuntimePath ./build/Studio_Bundle_Release/oae_audio.dll
```

Os ZIPs do Studio e das fontes saem em `build/releases/v0.1.1/`. O pacote Unity sai em `build/unity-releases/v0.1.1/`. No Studio, abra `example_project/audio-playground/playground_project.json` para testar a música e os efeitos, ou `playground_vo_project.json` na mesma pasta para ouvir o diálogo. Os créditos e a origem de cada WAV estão em [AUDIO-CREDITS.md](example_project/audio-playground/AUDIO-CREDITS.md).

### Pacote Unity e demo jogável

Em [Releases](https://github.com/soniwill/OpenAudioStudio/releases), baixe `OpenAudioUnity-<versão>-windows-x64.zip` para integrar o motor em um projeto Unity. O pacote contém `com.openaudio.core` e a DLL `oae_audio.dll` de Windows x64. Ele pode ser usado no Editor para Windows x64 e em builds Windows x64.

1. Extraia o ZIP para uma pasta que você possa manter junto ao projeto. No Unity, abra **Window → Package Manager**, selecione **Add package from disk** e escolha `com.openaudio.core/package.json`.
2. Publique um banco no OpenAudio Studio e copie o `.bank` e o respectivo `.bank.catalog.json` para `Assets/StreamingAssets/` do seu jogo.
3. Adicione `OaeAudioManager` e `StudioBankLoader` à cena. Configure o nome do banco no carregador e escolha um evento com `StudioEventEmitter`. Veja o [exercício de primeiro som](docs/manual/chapters/09-unity-componentes-inspector.html#primeiro-som).

Para experimentar a demo sem instalar o Unity Editor, extraia `OpenAudioStudio-AudioPlayground-<versão>-windows-x64.zip`, abra `AudioPlayground.exe` e pressione **E** para ouvir as falas. A demo tem os próprios bancos e inclui os créditos de áudio.

Para recompilar a demo localmente, use o Unity Editor 6000.0.34f1 ativado e o módulo Windows Build Support. Parta de um build Release do Studio. O comando recompila os bancos a partir dos mesmos WAVs versionados, incluindo as três falas Piper:

```powershell
./scripts/build_audio_playground.ps1 -Version v0.1.1
```

O comando gera a demo e a integração Unity em `build/unity-releases/v0.1.1/`. Se você rodou antes o empacotamento só da integração para a mesma versão, use uma pasta de build limpa para gerar o conjunto completo.

## Licenças

- O código próprio do motor, da API C, do compilador de bancos e do pacote Unity usa [Apache-2.0](LICENSE). Essa é a licença padrão para o código próprio deste repositório.
- O código próprio em `studio/` usa [GNU AGPL-3.0-only](studio/LICENSE). O executável do Studio é ligado ao JUCE 8. Para distribuir esse executável pela opção de código aberto do JUCE, disponibilize o código-fonte correspondente conforme a AGPL-3.0 e preserve os avisos aplicáveis. O runtime distribuído separadamente com a Unity não depende do JUCE.
- As dependências mantêm seus próprios termos. Consulte o [inventário nativo](native/external/README.md), os [avisos do runtime no pacote Unity](<unity/com.openaudio.core/THIRD PARTY NOTICES.md>) e o [licenciamento do JUCE 8](https://juce.com/legal/juce-8-licence/). Esta seção trata da licença do código e não concede direitos sobre mídias de terceiros.
