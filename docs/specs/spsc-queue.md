# Fila SPSC

`oae::SpscQueue<T, Capacity>` transporta comandos entre uma thread produtora e uma consumidora. Cada lado tem uso exclusivo de uma operação. A fila possui capacidade fixa, usa armazenamento próprio e não aloca durante push ou pop. Uma tentativa retorna imediatamente. Quando cheia, a fila recusa a escrita. Quando vazia, preserva a variável de saída.

O produtor publica o índice de escrita depois de gravar o comando. O consumidor publica o índice de leitura depois de copiar o comando. As operações usam pares release/acquire para que cada lado veja os dados antes de ler ou reutilizar um slot. `T` deve ser trivialmente copiável e aceitar atribuição por cópia sem exceção.

```cpp
oae::SpscQueue<int, 16> commands;
bool queued = commands.try_push(42);
int command = 0;
bool received = commands.try_pop(command);
```

Em execução concorrente, chame `try_push` só no produtor e `try_pop` só no consumidor. Se a fila estiver cheia, trate a falha fora da thread de áudio. Se estiver vazia, a thread de áudio pode seguir com o estado atual. A fila precisa sobreviver às duas threads.

Execute `./scripts/build.ps1 -Config Debug` para rodar os testes de fila vazia, cheia, reutilização de slots e transferência ordenada de um milhão de itens entre duas threads.
