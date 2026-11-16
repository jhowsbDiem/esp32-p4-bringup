#include "teste_audio.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"

#define I2C_SDA_GPIO 7
#define I2C_SCL_GPIO 8
// O pino CE do ES8311 define o bit menos significativo do endereço, e o nível
// dele na placa não está documentado.
#define CODEC_ENDERECO_CE_BAIXO 0x18
#define CODEC_ENDERECO_CE_ALTO 0x19
#define I2C_TIMEOUT_MS 50

static const char *TAG = "teste_audio";

bool teste_audio_executar(void)
{
    const i2c_master_bus_config_t config = {
        .i2c_port = -1,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t barramento;
    esp_err_t err = i2c_new_master_bus(&config, &barramento);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar o barramento I2C: %s", esp_err_to_name(err));
        return false;
    }

    const uint16_t enderecos[] = { CODEC_ENDERECO_CE_BAIXO, CODEC_ENDERECO_CE_ALTO };
    bool encontrado = false;
    for (size_t i = 0; i < sizeof(enderecos) / sizeof(enderecos[0]); i++) {
        if (i2c_master_probe(barramento, enderecos[i], I2C_TIMEOUT_MS) == ESP_OK) {
            ESP_LOGI(TAG, "Codec ES8311 encontrado em 0x%02X (SDA GPIO%d, SCL GPIO%d)",
                     enderecos[i], I2C_SDA_GPIO, I2C_SCL_GPIO);
            encontrado = true;
            break;
        }
    }

    if (!encontrado) {
        ESP_LOGE(TAG, "Codec não respondeu em 0x%02X nem em 0x%02X",
                 CODEC_ENDERECO_CE_BAIXO, CODEC_ENDERECO_CE_ALTO);

        // A lista mostra se o problema está nos pinos (nenhum dispositivo) ou
        // no endereço (outro dispositivo respondeu).
        char lista[5 * (0x77 - 0x08 + 1) + 1] = "";
        size_t usado = 0;
        for (uint16_t endereco = 0x08; endereco <= 0x77; endereco++) {
            if (i2c_master_probe(barramento, endereco, I2C_TIMEOUT_MS) != ESP_OK) {
                continue;
            }
            int escrito = snprintf(lista + usado, sizeof(lista) - usado, " 0x%02X", endereco);
            if (escrito > 0 && (size_t)escrito < sizeof(lista) - usado) {
                usado += (size_t)escrito;
            }
        }
        ESP_LOGE(TAG, "Dispositivos no barramento:%s", usado > 0 ? lista : " nenhum");
    }

    i2c_del_master_bus(barramento);
    return encontrado;
}
