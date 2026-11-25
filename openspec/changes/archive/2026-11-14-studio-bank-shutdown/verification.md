# Verificação de studio-bank-shutdown

Execução em Windows, em 2026-11-14.

## Motor nativo

O teste `System release unloads only its own banks` usa o fixture de banco com amostra. Ele libera um sistema com um banco síncrono e uma carga não bloqueante, consulta os handles diretamente no pool e verifica pela API C que o outro sistema ainda pode consultar seu banco e evento, atualizar e descarregar explicitamente.

`./scripts/verify.ps1 -Layer native` terminou com código 0, com CTest 2/2 em Debug e 2/2 em Release. A execução inicial ficou parada em `Start 1: native` dentro do sandbox. A repetição fora do sandbox passou.

Para repetir a verificação de saída, salve o trecho abaixo como `build/bank_shutdown_probe.py`. Use Python de 64 bits e a DLL Release produzida pelo gate nativo.

```python
import ctypes as c
import sys
from pathlib import Path


class InitSettings(c.Structure):
    _fields_ = [
        ("sample_rate", c.c_uint32),
        ("channels", c.c_uint32),
        ("max_physical_voices", c.c_uint32),
        ("flags", c.c_uint32),
    ]


api = c.CDLL(sys.argv[1])
handle_type = c.c_uint64
api.oae_system_create.argtypes = [c.POINTER(handle_type)]
api.oae_system_init.argtypes = [handle_type, c.POINTER(InitSettings)]
api.oae_studio_load_bank_file.argtypes = [handle_type, c.c_char_p, c.POINTER(handle_type)]
api.oae_studio_unload_bank.argtypes = [handle_type, handle_type]
api.oae_system_release.argtypes = [handle_type]


def check(step, result):
    print(f"{step}: {result}", flush=True)
    if result != 0:
        raise SystemExit(10)


system = handle_type()
bank = handle_type()
settings = InitSettings(48000, 2, 32, 1)  # OAE_INIT_NO_DEVICE
bank_path = str(Path(sys.argv[2]).resolve()).encode("utf-8")
check("create", api.oae_system_create(c.byref(system)))
check("init_no_device", api.oae_system_init(system, c.byref(settings)))
check("load_bank", api.oae_studio_load_bank_file(system, bank_path, c.byref(bank)))
if sys.argv[3] == "unload":
    check("unload_bank", api.oae_studio_unload_bank(system, bank))
check("release", api.oae_system_release(system))
```

Execute da raiz do repositório, em processos separados.

```powershell
$dll = (Resolve-Path 'build/Release/Release/oae_audio.dll').Path
$bank = (Resolve-Path 'example_project/build/master.bank').Path
python build/bank_shutdown_probe.py $dll $bank loaded
$LASTEXITCODE
python build/bank_shutdown_probe.py $dll $bank unload
$LASTEXITCODE
```

Todas as chamadas retornaram `OAE_OK` (0) e os dois processos terminaram com código 0. No controle, `unload_bank` retornou 0 antes de `release`.

## Fechamento e reabertura do Studio

1. Depois de compilar o Studio Release, copie `OpenAudio Studio.exe`, `oae_audio.dll` e `oae_bankc.exe` de `build/Studio_Release/studio/oae_studio_artefacts/Release/` para uma pasta de teste nova. Copie também a pasta `example_project` da raiz para essa pasta. O Studio usa o diretório do executável para gravar `oae_studio_layout.json`.
2. Abra a janela principal com `"OpenAudio Studio.exe" --mcp-port=17451 "<cópia>\example_project\project.json"`. Aguarde `[Preview] Live preview hot swapped.` no log de diagnóstico. O teste consultou `oae://diagnostics/log` pelo MCP local e confirmou o caminho do projeto e a inicialização da saída de áudio.
3. Com o layout `Default (Authoring)`, clique em `+ Save` e aceite o nome padrão `Custom 1`. Confirme a seleção e que Browser e Bottom Deck estão visíveis. Selecione `Stage Focus` e confirme que os dois painéis estão ocultos.
4. Feche a janela principal com o projeto aberto. No teste automatizado, o processo foi criado com `Start-Process -PassThru` e fechado com `CloseMainWindow()`. Depois de `WaitForExit()`, `ExitCode` foi 0.
5. Execute o mesmo comando de abertura e aguarde a mesma confirmação de prévia. Selecione `Custom 1` no seletor de workspace. Browser e Bottom Deck voltaram a ficar visíveis. Feche a janela principal e confira `ExitCode` 0 novamente.

O arquivo de layout da cópia de teste continha `Custom 1` após o segundo fechamento. Nesta execução, o roteiro local `build/studio-bank-shutdown-accept/Run-Acceptance.ps1` registrou as duas sessões, a inspeção dos painéis e os códigos de saída em `build/studio-bank-shutdown-accept/run-20261114-214128/acceptance.jsonl`.

## Gates

| Comando | Resultado |
| --- | --- |
| `./scripts/verify.ps1 -Layer studio` | Código 0, Release, 11/11 smokes. |
| `./scripts/verify.ps1 -Layer docs` | Código 0, 9/9 artefatos OpenSpec e links relativos aprovados. |
| `openspec validate studio-bank-shutdown --strict` | Código 0, change válida. |

O gate do Studio foi executado fora do sandbox após o Git/MSYS retornar acesso negado na checagem do submódulo JUCE. Na primeira execução, o `--smoke-pro-ui` encontrou o preset persistido `Mixing & Mastering`, que deixava a Timeline oculta. O smoke espera que todos os painéis estejam visíveis após restaurar. O gate completo passou partindo do layout padrão, com `oae_studio_layout.json` guardado temporariamente e restaurado após o teste. A conferência por hash confirmou os mesmos bytes após a restauração.
