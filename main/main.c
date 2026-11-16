#include <stdbool.h>
#include <stdio.h>

#include "esp_log.h"
#include "teste_chip.h"
#include "teste_psram.h"
#include "teste_audio.h"

static const char *TAG = "bringup";

typedef struct {
    const char *nome;
    bool (*executar)(void);
} teste_t;

// Novos testes entram só como uma linha a mais nesta tabela.
static const teste_t testes[] = {
    { "chip", teste_chip_executar },
    { "psram", teste_psram_executar },
    { "psram_vel", teste_psram_velocidade_executar },
    { "audio", teste_audio_executar },
};

void app_main(void)
{
    ESP_LOGI(TAG, "Firmware de teste da ESP32-P4-Module-DEV-KIT iniciado");

    char resumo[128] = "";
    size_t usado = 0;
    bool todos_ok = true;

    for (size_t i = 0; i < sizeof(testes) / sizeof(testes[0]); i++) {
        bool ok = testes[i].executar();
        if (ok) {
            ESP_LOGI(TAG, "%s: OK", testes[i].nome);
        } else {
            ESP_LOGE(TAG, "%s: FALHA", testes[i].nome);
            todos_ok = false;
        }

        if (usado < sizeof(resumo)) {
            int escrito = snprintf(resumo + usado, sizeof(resumo) - usado, "%s%s=%s",
                                   i > 0 ? " " : "", testes[i].nome, ok ? "OK" : "FALHA");
            if (escrito > 0) {
                usado += (size_t)escrito;
            }
        }
    }

    if (todos_ok) {
        ESP_LOGI(TAG, "Resumo: %s", resumo);
    } else {
        ESP_LOGE(TAG, "Resumo: %s", resumo);
    }
}
