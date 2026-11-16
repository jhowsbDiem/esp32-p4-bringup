#pragma once

#include <stdbool.h>

/**
 * Imprime o tamanho da PSRAM e a memória livre nela. Retorna true quando a
 * PSRAM foi inicializada com o tamanho esperado de 32 MB.
 */
bool teste_psram_executar(void);

/**
 * Mede a velocidade de escrita, leitura e cópia na PSRAM e na RAM interna e
 * imprime em MB/s. Retorna true quando as alocações dão certo e a cópia
 * confere.
 */
bool teste_psram_velocidade_executar(void);
