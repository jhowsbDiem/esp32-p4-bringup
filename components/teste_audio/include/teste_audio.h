#pragma once

#include <stdbool.h>

/**
 * Procura o codec ES8311 no barramento I2C. Retorna true quando ele responde
 * num dos dois endereços possíveis.
 */
bool teste_audio_executar(void);
