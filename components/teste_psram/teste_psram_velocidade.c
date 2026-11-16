#include "teste_psram.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

// Bem maior que o cache L2 de 128 KB, para que a medição reflita a PSRAM e
// não o cache.
#define BLOCO_PSRAM_BYTES (4 * 1024 * 1024)
#define BLOCO_INTERNO_BYTES (128 * 1024)
// As duas memórias movem o mesmo volume, para que o bloco pequeno da RAM
// interna não seja cronometrado num intervalo curto demais.
#define BYTES_POR_MEDICAO (16 * 1024 * 1024)

static const char *TAG = "teste_psram_vel";

// Sem um destino volátil, o compilador poderia eliminar o laço de leitura, já
// que o resultado dele não é usado.
static volatile uint32_t soma_descartada;

static uint32_t mb_por_segundo(uint64_t bytes, int64_t us)
{
    if (us <= 0) {
        return 0;
    }
    return (uint32_t)((bytes * 1000000) / ((uint64_t)us * 1024 * 1024));
}

static bool medir_memoria(const char *nome, uint32_t caps, size_t tamanho)
{
    uint8_t *bloco = heap_caps_aligned_alloc(64, tamanho, caps);
    if (bloco == NULL) {
        ESP_LOGE(TAG, "Sem memória para o bloco de %zu KB em %s", tamanho / 1024, nome);
        return false;
    }

    size_t repeticoes = BYTES_POR_MEDICAO / tamanho;

    int64_t inicio = esp_timer_get_time();
    for (size_t i = 0; i < repeticoes; i++) {
        memset(bloco, (int)(i & 0xFF), tamanho);
    }
    int64_t us_escrita = esp_timer_get_time() - inicio;

    uint32_t soma = 0;
    inicio = esp_timer_get_time();
    for (size_t i = 0; i < repeticoes; i++) {
        const uint32_t *palavras = (const uint32_t *)bloco;
        for (size_t j = 0; j < tamanho / sizeof(uint32_t); j++) {
            soma += palavras[j];
        }
    }
    int64_t us_leitura = esp_timer_get_time() - inicio;
    soma_descartada = soma;

    size_t metade = tamanho / 2;
    memset(bloco, 0xA5, metade);
    memset(bloco + metade, 0x5A, metade);
    inicio = esp_timer_get_time();
    for (size_t i = 0; i < repeticoes; i++) {
        memcpy(bloco + metade, bloco, metade);
    }
    int64_t us_copia = esp_timer_get_time() - inicio;

    // As metades começam com padrões diferentes, então elas só ficam iguais
    // se a cópia de fato aconteceu.
    if (memcmp(bloco, bloco + metade, metade) != 0) {
        ESP_LOGE(TAG, "Cópia não conferiu em %s", nome);
        heap_caps_free(bloco);
        return false;
    }

    heap_caps_free(bloco);

    uint64_t bytes_escrita_leitura = (uint64_t)tamanho * repeticoes;
    uint64_t bytes_copia = (uint64_t)metade * repeticoes;
    ESP_LOGI(TAG, "%s (%zu KB): escrita %" PRIu32 " MB/s | leitura %" PRIu32
                  " MB/s | cópia %" PRIu32 " MB/s",
             nome, tamanho / 1024,
             mb_por_segundo(bytes_escrita_leitura, us_escrita),
             mb_por_segundo(bytes_escrita_leitura, us_leitura),
             mb_por_segundo(bytes_copia, us_copia));
    return true;
}

bool teste_psram_velocidade_executar(void)
{
    int64_t inicio = esp_timer_get_time();

    bool ok = medir_memoria("PSRAM", MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT, BLOCO_PSRAM_BYTES);
    ok = medir_memoria("RAM interna", MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT, BLOCO_INTERNO_BYTES) && ok;

    int64_t fim = esp_timer_get_time();
    ESP_LOGI(TAG, "Tempo total: %" PRId64 " ms", (fim - inicio) / 1000);
    return ok;
}
