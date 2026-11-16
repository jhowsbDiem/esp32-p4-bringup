#pragma once

#include <stdbool.h>

/**
 * Encontra o codec ES8311 no I2C, configura o I2S, abre o codec e toca um tom
 * curto no alto-falante. Retorna true quando tudo isso dá certo.
 */
bool teste_audio_executar(void);
