#pragma once

#include <stdbool.h>

/**
 * Encontra o codec ES8311 no I2C, configura o I2S e abre o codec. Retorna true
 * quando tudo isso dá certo.
 */
bool teste_audio_executar(void);
