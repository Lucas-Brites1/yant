## ctx->strings está morto

Estado atual: ctx->strings é arena alocada em yant_context_init mas
nunca usada. Lexemes apontam diretamente pra Source.text.

Decisões possíveis:
- (A) Deletar ctx->strings. Strings dinâmicas vão pra ctx->runtime.
- (B) Repurpose pra strings dinâmicas (renomear pra clareza).
- (C) Implementar internalização real com dedup.

Recomendação: A primeiro. C quando lifetime de Source.text virar limitação
(REPL, imports) ou comparação de slices virar gargalo perfilado.

Não fazer agora — débito documentado. Implementar junto com primeiro caso
real de string dinâmica (concat, format).


## Named arguments (v0.2+)

Permitir:
- soma(:x 10, :y 20)            // named
- soma(10, 20)                   // positional (default v0.1)
- soma(10, :y 20)                // misturado (positional first, then named)

Requer:
- Tag ArgKind no parser
- Validação: positional antes de named
- Validação: sem duplicatas
- Resolução: matching de :name pra params

Pré-requisitos:
- Função funcionando com positional simples (v0.1)
- Match implementado (v0.1)
- Loop implementado (v0.1)

## Performance — observações iniciais

fib(30) recursivo ingênuo: ~3-4 segundos (~2.7M chamadas).
fatorial(19): instantâneo (~19 chamadas).

Gargalos prováveis:
- enter_scope/exit_scope (calloc/free do hashmap por chamada)
- value_alloc na arena runtime
- vec_of/vec_free de arg_values

Otimizações futuras (não fazer ainda):
- Pool de scopes reusáveis
- Bytecode VM (Crafting Interpreters parte 2)
- Stack-based locals em vez de hashmap por função

Decisão: aceitar performance atual pra v0.1. Yant é didática,
não competidora de Lua/Python. Otimizar quando programar Yant
"em escala" virar dor real.
