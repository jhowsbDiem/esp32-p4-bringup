#pragma once

#include <stdbool.h>

/**
 * Encontra o codec ES8311 no I2C, configura o I2S, abre o codec, toca um tom
 * curto e confere pelo microfone que o tom foi captado acima do silêncio.
 * Retorna true quando tudo isso dá certo.
 */
bool teste_audio_executar(void);
