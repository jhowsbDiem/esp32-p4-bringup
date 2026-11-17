#include "teste_audio.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "audio_placa.h"
#include "esp_codec_dev.h"
#include "esp_log.h"

#define AUDIO_TOM_HZ 1000
#define AUDIO_TOM_MS 1000
// O silêncio no final empurra o fim do tom para fora do DMA (cerca de 90 ms
// enfileirados) antes de o codec fechar e desligar o amplificador. Sem ele, o
// tom seria cortado no meio.
#define AUDIO_SILENCIO_MS 100
#define AUDIO_BLOCO_MS 10
// O sinal digital fica com folga (8192 de 32767, cerca de -12 dBFS) para não
// distorcer; quem dá a altura do som é o ganho do codec (0 a 100), ajustado
// na bancada.
#define AUDIO_VOLUME 90
#define AUDIO_AMPLITUDE 8192
#define AUDIO_FRAMES_POR_BLOCO (AUDIO_PLACA_TAXA_HZ * AUDIO_BLOCO_MS / 1000)

#define AUDIO_GANHO_MIC_DB 18
// O ADC do ES8311 leva cerca de 600 ms para acomodar depois de ligar; antes
// disso o nível contínuo ainda está caindo.
#define AUDIO_ACOMODACAO_MS 700
#define AUDIO_SILENCIO_MEDIDO_MS 300
// No tom, o início traz dado velho da fila do DMA e o atraso até o som sair do
// alto-falante.
#define AUDIO_DESCARTE_MS 200
// Quanto o tom precisa ficar acima do silêncio para contar como ouvido;
// ajustado na bancada, com e sem o alto-falante conectado.
#define AUDIO_LIMIAR_DB 20
// Abaixo deste valor o nível é tratado como zero, para o log10 não ir a menos
// infinito.
#define AUDIO_AMPLITUDE_PISO 0.001f
#define AUDIO_BLOCOS_SILENCIO (AUDIO_SILENCIO_MEDIDO_MS / AUDIO_BLOCO_MS)
#define AUDIO_BLOCOS_TOM ((AUDIO_TOM_MS - AUDIO_DESCARTE_MS) / AUDIO_BLOCO_MS)
#define AUDIO_MAX_BLOCOS_MEDIDOS \
    (AUDIO_BLOCOS_TOM > AUDIO_BLOCOS_SILENCIO ? AUDIO_BLOCOS_TOM : AUDIO_BLOCOS_SILENCIO)

// Com períodos inteiros, o bloco se repete sem emenda e o tom começa e termina
// no zero, sem estalo.
_Static_assert((AUDIO_TOM_HZ * AUDIO_BLOCO_MS) % 1000 == 0,
               "o bloco precisa conter um número inteiro de períodos do tom");

static const char *TAG = "teste_audio";

typedef struct {
    float amplitudes[AUDIO_MAX_BLOCOS_MEDIDOS];
    int blocos;
} medicao_t;

static int16_t leitura[AUDIO_FRAMES_POR_BLOCO * 2];

// Goertzel: mede só a amplitude da frequência do tom. Ruído espalhado pelas
// outras frequências e a deriva lenta do ADC ficam de fora, e o bloco com
// períodos inteiros põe o tom exatamente no centro da faixa medida.
static float amplitude_do_tom(const int16_t *bloco)
{
    const float coeficiente =
        2.0f * cosf(2.0f * (float)M_PI * AUDIO_TOM_HZ / AUDIO_PLACA_TAXA_HZ);
    float s1 = 0.0f;
    float s2 = 0.0f;
    // O canal direito traz a cópia interna do DAC, que mostraria o tom mesmo
    // sem alto-falante; só o esquerdo vem do microfone.
    for (int i = 0; i < AUDIO_FRAMES_POR_BLOCO; i++) {
        float s = bloco[2 * i] + coeficiente * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    float potencia = s1 * s1 + s2 * s2 - coeficiente * s1 * s2;
    return 2.0f * sqrtf(fmaxf(potencia, 0.0f)) / AUDIO_FRAMES_POR_BLOCO;
}

static void registrar_bloco(medicao_t *medicao, const int16_t *bloco)
{
    if (medicao->blocos < AUDIO_MAX_BLOCOS_MEDIDOS) {
        medicao->amplitudes[medicao->blocos] = amplitude_do_tom(bloco);
        medicao->blocos++;
    }
}

static bool medir_silencio(esp_codec_dev_handle_t codec, medicao_t *silencio)
{
    int resultado = esp_codec_dev_set_in_gain(codec, AUDIO_GANHO_MIC_DB);
    if (resultado != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Falha ao ajustar o ganho do microfone: erro %d", resultado);
        return false;
    }

    const int descartados = AUDIO_ACOMODACAO_MS / AUDIO_BLOCO_MS;
    const int total = descartados + AUDIO_BLOCOS_SILENCIO;
    for (int n = 0; n < total; n++) {
        resultado = esp_codec_dev_read(codec, leitura, sizeof(leitura));
        if (resultado != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Falha ao ler do I2S: erro %d", resultado);
            return false;
        }
        if (n >= descartados) {
            registrar_bloco(silencio, leitura);
        }
    }
    return true;
}

static bool tocar_tom(esp_codec_dev_handle_t codec, medicao_t *tom)
{
    static int16_t bloco[AUDIO_FRAMES_POR_BLOCO * 2];

    int resultado = esp_codec_dev_set_out_vol(codec, AUDIO_VOLUME);
    if (resultado != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Falha ao ajustar o volume: erro %d", resultado);
        return false;
    }

    for (int i = 0; i < AUDIO_FRAMES_POR_BLOCO; i++) {
        int16_t amostra = (int16_t)lrintf(
            AUDIO_AMPLITUDE * sinf(2.0f * (float)M_PI * AUDIO_TOM_HZ * i / AUDIO_PLACA_TAXA_HZ));
        bloco[2 * i] = amostra;
        bloco[2 * i + 1] = amostra;
    }

    ESP_LOGI(TAG, "Tocando tom de %d Hz por %d ms (volume %d)", AUDIO_TOM_HZ, AUDIO_TOM_MS,
             AUDIO_VOLUME);
    for (int n = 0; n < AUDIO_TOM_MS / AUDIO_BLOCO_MS; n++) {
        resultado = esp_codec_dev_write(codec, bloco, sizeof(bloco));
        if (resultado != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Falha ao escrever no I2S: erro %d", resultado);
            return false;
        }

        // Ler um bloco por bloco escrito mantém gravação e reprodução no mesmo
        // ritmo; a leitura espera os 10 ms, e a escrita fica adiantada no máximo
        // a fila do DMA.
        resultado = esp_codec_dev_read(codec, leitura, sizeof(leitura));
        if (resultado != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Falha ao ler do I2S: erro %d", resultado);
            return false;
        }
        if (n >= AUDIO_DESCARTE_MS / AUDIO_BLOCO_MS) {
            registrar_bloco(tom, leitura);
        }
    }

    memset(bloco, 0, sizeof(bloco));
    for (int n = 0; n < AUDIO_SILENCIO_MS / AUDIO_BLOCO_MS; n++) {
        resultado = esp_codec_dev_write(codec, bloco, sizeof(bloco));
        if (resultado != ESP_CODEC_DEV_OK) {
            ESP_LOGE(TAG, "Falha ao escrever no I2S: erro %d", resultado);
            return false;
        }
    }
    return true;
}

static int comparar_amplitudes(const void *a, const void *b)
{
    float x = *(const float *)a;
    float y = *(const float *)b;
    return (x > y) - (x < y);
}

// A mediana só sobe quando mais da metade dos blocos tem o tom; ele é contínuo,
// enquanto voz ou palmas chegam em rajadas.
static float mediana(medicao_t *medicao)
{
    if (medicao->blocos == 0) {
        return 0.0f;
    }
    qsort(medicao->amplitudes, (size_t)medicao->blocos, sizeof(medicao->amplitudes[0]),
          comparar_amplitudes);
    return medicao->amplitudes[medicao->blocos / 2];
}

static int em_dbfs(float amplitude)
{
    return (int)lrintf(20.0f * log10f(fmaxf(amplitude, AUDIO_AMPLITUDE_PISO) / 32768.0f));
}

static bool conferir_microfone(medicao_t *silencio, medicao_t *tom)
{
    int silencio_dbfs = em_dbfs(mediana(silencio));
    int tom_dbfs = em_dbfs(mediana(tom));
    int relacao_db = tom_dbfs - silencio_dbfs;

    ESP_LOGI(TAG, "Microfone em %d Hz: silêncio %d dBFS | tom %d dBFS | relação %d dB "
                  "(mínimo %d dB)",
             AUDIO_TOM_HZ, silencio_dbfs, tom_dbfs, relacao_db, AUDIO_LIMIAR_DB);
    if (relacao_db < AUDIO_LIMIAR_DB) {
        ESP_LOGE(TAG, "O microfone não captou o tom acima do silêncio");
        return false;
    }
    return true;
}

bool teste_audio_executar(void)
{
    // Estáticas para não pesar na pilha da main_task.
    static medicao_t silencio;
    static medicao_t tom;
    silencio.blocos = 0;
    tom.blocos = 0;
    esp_codec_dev_handle_t codec = audio_placa_abrir();
    bool ok = codec != NULL && medir_silencio(codec, &silencio) && tocar_tom(codec, &tom)
              && conferir_microfone(&silencio, &tom);
    audio_placa_fechar();
    return ok;
}
