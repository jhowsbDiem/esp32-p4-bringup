#include "teste_psram.h"

#include <stddef.h>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"

#define PSRAM_ESPERADA_BYTES (32 * 1024 * 1024)

static const char *TAG = "teste_psram";

bool teste_psram_executar(void)
{
    // Com a configuração atual o boot aborta antes de chegar aqui se a PSRAM
    // falhar; a checagem cobre uma configuração que permita continuar sem
    // PSRAM (CONFIG_SPIRAM_IGNORE_NOTFOUND).
    if (!esp_psram_is_initialized()) {
        ESP_LOGE(TAG, "PSRAM não inicializada");
        return false;
    }

    size_t tamanho = esp_psram_get_size();
    size_t livre = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t maior_bloco = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "PSRAM: %zu MB | livre no heap: %zu KB | maior bloco livre: %zu KB",
             tamanho / (1024 * 1024), livre / 1024, maior_bloco / 1024);

    if (tamanho != PSRAM_ESPERADA_BYTES) {
        ESP_LOGE(TAG, "PSRAM de %zu MB diferente dos %d MB esperados",
                 tamanho / (1024 * 1024), PSRAM_ESPERADA_BYTES / (1024 * 1024));
        return false;
    }
    return true;
}
