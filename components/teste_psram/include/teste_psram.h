#pragma once

#include <stdbool.h>

/**
 * Imprime o tamanho da PSRAM e a memória livre nela. Retorna true quando a
 * PSRAM foi inicializada com o tamanho esperado de 32 MB.
 */
bool teste_psram_executar(void);
