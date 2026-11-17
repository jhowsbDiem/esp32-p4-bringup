#pragma once

#include "esp_codec_dev.h"

#define AUDIO_PLACA_TAXA_HZ 16000

/**
 * Abre o áudio da placa: cria o barramento I2C, encontra o codec ES8311, cria
 * os canais I2S TX e RX e abre o codec, o que liga o amplificador. O formato é
 * AUDIO_PLACA_TAXA_HZ, 16 bits, estéreo intercalado (esquerdo, direito); na
 * leitura, o canal esquerdo é o microfone e o direito é uma cópia interna do
 * DAC. Retorna o dispositivo do codec, ou NULL se alguma etapa falhar, com tudo
 * o que foi criado já liberado. Não pode ser chamada de novo antes de
 * audio_placa_fechar().
 */
esp_codec_dev_handle_t audio_placa_abrir(void);

/**
 * Fecha o codec, o que desliga o amplificador, e libera os canais I2S e o
 * barramento I2C. Não faz nada se o áudio não estiver aberto.
 */
void audio_placa_fechar(void);
