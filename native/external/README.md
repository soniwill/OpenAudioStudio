# Dependências vendorizadas

Os arquivos foram copiados sem modificações. O build funciona sem baixar dependências. `stb_vorbis.c` veio da mesma versão do miniaudio para manter a integração Vorbis compatível.

| Arquivo | Origem | SHA-256 |
| --- | --- | --- |
| `miniaudio.h` | [Fonte fixada](https://raw.githubusercontent.com/mackron/miniaudio/0.11.26/miniaudio.h) | `6b376c7fbb1352ba1e0e432ab66069b335ea9405e45cb6c30b97ef0f3c9c3e0e` |
| `stb_vorbis.c` | [Fonte fixada](https://raw.githubusercontent.com/mackron/miniaudio/0.11.26/extras/stb_vorbis.c) | `4c7cb2ff1f7011e9d67950446b7eb9ca044f2e464d76bfbb0b84dd2e23e65636` |
| `doctest.h` | [Fonte fixada](https://raw.githubusercontent.com/doctest/doctest/v2.4.12/doctest/doctest.h) | `94029a7d32da24a56249658147dbd2b33ff0b9ed665295cbbaf19aafff5b0ced` |
| `nlohmann/json.hpp` | [Fonte fixada](https://raw.githubusercontent.com/nlohmann/json/v3.12.0/single_include/nlohmann/json.hpp) | `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63` |
| `licenses/miniaudio.txt` | [Fonte fixada](https://raw.githubusercontent.com/mackron/miniaudio/0.11.26/LICENSE) | `457f1b500e0adf6bc059edddfa78a2f62012e7c3bb43476c20e0bd23b25ba0eb` |
| `licenses/doctest.txt` | [Fonte fixada](https://raw.githubusercontent.com/doctest/doctest/v2.4.12/LICENSE.txt) | `0fe0b331fa1513dcce8604ff1fa925f32d1cea17d8aeb1c2471fad40d291adc5` |
| `licenses/nlohmann-json.txt` | [Fonte fixada](https://raw.githubusercontent.com/nlohmann/json/v3.12.0/LICENSE.MIT) | `46a65cffd1ea955132d95a8dd921640714a8d6b537d2e4e482d31145ae95b603` |

O miniaudio oferece MIT No Attribution ou domínio público. stb_vorbis oferece MIT ou domínio público, com os termos no próprio arquivo. doctest e nlohmann/json usam MIT. Os textos originais estão nos arquivos de origem e em `licenses/`.

Exemplo de verificação no PowerShell, a partir da raiz:

```powershell
Get-FileHash -Algorithm SHA256 ./native/external/miniaudio.h
```

O MSVC 19.43 emite C4701 em `seek_to_sample_coarse`. O segundo probe lê `mid` somente depois de uma iteração anterior bem-sucedida, que preenche `mid` antes de incrementar `probe`. A unidade C suprime C4701 apenas durante a inclusão da implementação vendorizada.
