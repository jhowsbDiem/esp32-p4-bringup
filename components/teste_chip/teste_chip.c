#include "teste_chip.h"

#include <inttypes.h>
#include <string.h>

#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "teste_chip";

bool teste_chip_executar(void)
{
    esp_chip_info_t info;
    esp_chip_info(&info);
    ESP_LOGI(TAG, "Modelo: %s | núcleos: %d | revisão: v%d.%d",
             CONFIG_IDF_TARGET, info.cores, info.revision / 100, info.revision % 100);

    static const struct {
        uint32_t flag;
        const char *nome;
    } recursos[] = {
        { CHIP_FEATURE_EMB_FLASH, "flash embutida" },
        { CHIP_FEATURE_EMB_PSRAM, "PSRAM embutida" },
        { CHIP_FEATURE_WIFI_BGN, "Wi-Fi 2.4 GHz" },
        { CHIP_FEATURE_BLE, "Bluetooth LE" },
        { CHIP_FEATURE_BT, "Bluetooth clássico" },
        { CHIP_FEATURE_IEEE802154, "IEEE 802.15.4" },
    };

    char lista[128] = "";
    for (size_t i = 0; i < sizeof(recursos) / sizeof(recursos[0]); i++) {
        if (info.features & recursos[i].flag) {
            if (lista[0] != '\0') {
                strlcat(lista, ", ", sizeof(lista));
            }
            strlcat(lista, recursos[i].nome, sizeof(lista));
        }
    }
    ESP_LOGI(TAG, "Recursos: %s", lista[0] != '\0' ? lista : "nenhum");

    uint32_t fisico = 0;
    uint32_t configurado = 0;
    esp_err_t err = esp_flash_get_physical_size(NULL, &fisico);
    if (err == ESP_OK) {
        err = esp_flash_get_size(NULL, &configurado);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao ler o tamanho da flash: %s", esp_err_to_name(err));
        return false;
    }

    uint32_t fisico_mb = fisico / (1024 * 1024);
    uint32_t configurado_mb = configurado / (1024 * 1024);
    ESP_LOGI(TAG, "Flash: física %" PRIu32 " MB | configurada %" PRIu32 " MB",
             fisico_mb, configurado_mb);
    ESP_LOGI(TAG, "ESP-IDF: %s", esp_get_idf_version());
    ESP_LOGI(TAG, "Heap interno livre: %zu bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    // O tamanho configurado vem do cabeçalho do binário e o físico do ID do chip
    // de memória; divergência indica configuração errada para a placa.
    if (fisico != configurado) {
        ESP_LOGE(TAG, "Flash física (%" PRIu32 " MB) diferente da configurada (%" PRIu32 " MB)",
                 fisico_mb, configurado_mb);
        return false;
    }
    return true;
}
