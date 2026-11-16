#include "teste_audio.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_err.h"
#include "esp_log.h"

#define I2C_PORTA I2C_NUM_0
#define I2C_SDA_GPIO 7
#define I2C_SCL_GPIO 8
// O pino CE do ES8311 define o bit menos significativo do endereço, e o nível
// dele na placa não está documentado.
#define CODEC_ENDERECO_CE_BAIXO 0x18
#define CODEC_ENDERECO_CE_ALTO 0x19
#define I2C_TIMEOUT_MS 50

#define I2S_PORTA I2S_NUM_0
#define I2S_MCLK_GPIO 13
#define I2S_BCLK_GPIO 12
#define I2S_WS_GPIO 10
#define I2S_DOUT_GPIO 9
#define I2S_DIN_GPIO 11

#define AUDIO_TAXA_HZ 16000
// O I2S e o ES8311 precisam concordar na relação MCLK/LRCK, e 256 basta para
// 16 bits.
#define AUDIO_MCLK_DIV 256
#define AUDIO_PA_GPIO 53

static const char *TAG = "teste_audio";

typedef struct {
    i2c_master_bus_handle_t barramento;
    i2s_chan_handle_t i2s_tx;
    i2s_chan_handle_t i2s_rx;
    const audio_codec_ctrl_if_t *ctrl_if;
    const audio_codec_gpio_if_t *gpio_if;
    const audio_codec_data_if_t *data_if;
    const audio_codec_if_t *codec_if;
    esp_codec_dev_handle_t codec;
} audio_t;

static bool criar_barramento(audio_t *audio)
{
    const i2c_master_bus_config_t config = {
        .i2c_port = I2C_PORTA,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&config, &audio->barramento);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar o barramento I2C: %s", esp_err_to_name(err));
        audio->barramento = NULL;
        return false;
    }
    return true;
}

static bool detectar_codec(i2c_master_bus_handle_t barramento, uint16_t *endereco)
{
    const uint16_t enderecos[] = { CODEC_ENDERECO_CE_BAIXO, CODEC_ENDERECO_CE_ALTO };
    bool encontrado = false;
    for (size_t i = 0; i < sizeof(enderecos) / sizeof(enderecos[0]); i++) {
        if (i2c_master_probe(barramento, enderecos[i], I2C_TIMEOUT_MS) == ESP_OK) {
            ESP_LOGI(TAG, "Codec ES8311 encontrado em 0x%02X (SDA GPIO%d, SCL GPIO%d)",
                     enderecos[i], I2C_SDA_GPIO, I2C_SCL_GPIO);
            *endereco = enderecos[i];
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
        for (uint16_t sonda = 0x08; sonda <= 0x77; sonda++) {
            if (i2c_master_probe(barramento, sonda, I2C_TIMEOUT_MS) != ESP_OK) {
                continue;
            }
            int escrito = snprintf(lista + usado, sizeof(lista) - usado, " 0x%02X", sonda);
            if (escrito > 0 && (size_t)escrito < sizeof(lista) - usado) {
                usado += (size_t)escrito;
            }
        }
        ESP_LOGE(TAG, "Dispositivos no barramento:%s", usado > 0 ? lista : " nenhum");
    }

    return encontrado;
}

static bool criar_i2s(audio_t *audio)
{
    // Sem dados novos, o DMA envia zeros (silêncio) em vez de repetir o último
    // buffer.
    i2s_chan_config_t canais = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORTA, I2S_ROLE_MASTER);
    canais.auto_clear = true;

    esp_err_t err = i2s_new_channel(&canais, &audio->i2s_tx, &audio->i2s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao criar os canais I2S: %s", esp_err_to_name(err));
        audio->i2s_tx = NULL;
        audio->i2s_rx = NULL;
        return false;
    }

    const i2s_std_config_t modo = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_TAXA_HZ),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_MCLK_GPIO,
            .bclk = I2S_BCLK_GPIO,
            .ws = I2S_WS_GPIO,
            .dout = I2S_DOUT_GPIO,
            .din = I2S_DIN_GPIO,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    bool tx_habilitado = false;
    err = i2s_channel_init_std_mode(audio->i2s_tx, &modo);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar o modo padrão do I2S TX: %s", esp_err_to_name(err));
        goto desfazer;
    }
    err = i2s_channel_init_std_mode(audio->i2s_rx, &modo);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar o modo padrão do I2S RX: %s", esp_err_to_name(err));
        goto desfazer;
    }
    err = i2s_channel_enable(audio->i2s_tx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao habilitar o I2S TX: %s", esp_err_to_name(err));
        goto desfazer;
    }
    tx_habilitado = true;
    err = i2s_channel_enable(audio->i2s_rx);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao habilitar o I2S RX: %s", esp_err_to_name(err));
        goto desfazer;
    }
    return true;

desfazer:
    if (tx_habilitado) {
        i2s_channel_disable(audio->i2s_tx);
    }
    i2s_del_channel(audio->i2s_tx);
    i2s_del_channel(audio->i2s_rx);
    audio->i2s_tx = NULL;
    audio->i2s_rx = NULL;
    return false;
}

static bool abrir_codec(audio_t *audio, uint16_t endereco)
{
    // O esp_codec_dev espera o endereço em 8 bits (7 bits deslocados de um).
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = I2C_PORTA,
        .addr = (uint8_t)(endereco << 1),
        .bus_handle = audio->barramento,
    };
    audio->ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    if (audio->ctrl_if == NULL) {
        ESP_LOGE(TAG, "Falha ao criar a interface de controle I2C do codec");
        return false;
    }

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_PORTA,
        .rx_handle = audio->i2s_rx,
        .tx_handle = audio->i2s_tx,
    };
    audio->data_if = audio_codec_new_i2s_data(&i2s_cfg);
    if (audio->data_if == NULL) {
        ESP_LOGE(TAG, "Falha ao criar a interface de dados I2S do codec");
        return false;
    }

    audio->gpio_if = audio_codec_new_gpio();
    if (audio->gpio_if == NULL) {
        ESP_LOGE(TAG, "Falha ao criar a interface de GPIO do codec");
        return false;
    }

    es8311_codec_cfg_t es8311_cfg = {
        .ctrl_if = audio->ctrl_if,
        .gpio_if = audio->gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_BOTH,
        .master_mode = false,
        .use_mclk = true,
        .pa_pin = AUDIO_PA_GPIO,
        .pa_reverted = false,
        .mclk_div = AUDIO_MCLK_DIV,
    };
    audio->codec_if = es8311_codec_new(&es8311_cfg);
    if (audio->codec_if == NULL) {
        ESP_LOGE(TAG, "Falha ao criar o driver do ES8311");
        return false;
    }

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
        .codec_if = audio->codec_if,
        .data_if = audio->data_if,
    };
    audio->codec = esp_codec_dev_new(&dev_cfg);
    if (audio->codec == NULL) {
        ESP_LOGE(TAG, "Falha ao criar o dispositivo de codec");
        return false;
    }

    esp_codec_dev_sample_info_t formato = {
        .bits_per_sample = 16,
        .channel = 2,
        .channel_mask = 0x03,
        .sample_rate = AUDIO_TAXA_HZ,
    };
    int resultado = esp_codec_dev_open(audio->codec, &formato);
    if (resultado != ESP_CODEC_DEV_OK) {
        ESP_LOGE(TAG, "Falha ao abrir o codec: erro %d", resultado);
        return false;
    }

    ESP_LOGI(TAG, "Codec aberto: %d Hz, %d bits, %d canais", AUDIO_TAXA_HZ,
             formato.bits_per_sample, formato.channel);
    return true;
}

static void desabilitar_canal(i2s_chan_handle_t canal)
{
    // O esp_codec_dev já desabilita os canais ao fechar o codec; desabilitar de
    // novo gera erro no log.
    i2s_chan_info_t info;
    if (i2s_channel_get_info(canal, &info) == ESP_OK && info.is_enabled) {
        i2s_channel_disable(canal);
    }
}

static void liberar_audio(audio_t *audio)
{
    if (audio->codec != NULL) {
        esp_codec_dev_delete(audio->codec);
    }
    if (audio->codec_if != NULL) {
        audio_codec_delete_codec_if(audio->codec_if);
    }
    if (audio->data_if != NULL) {
        audio_codec_delete_data_if(audio->data_if);
    }
    if (audio->gpio_if != NULL) {
        audio_codec_delete_gpio_if(audio->gpio_if);
    }
    if (audio->ctrl_if != NULL) {
        audio_codec_delete_ctrl_if(audio->ctrl_if);
    }
    if (audio->i2s_tx != NULL) {
        desabilitar_canal(audio->i2s_tx);
        i2s_del_channel(audio->i2s_tx);
    }
    if (audio->i2s_rx != NULL) {
        desabilitar_canal(audio->i2s_rx);
        i2s_del_channel(audio->i2s_rx);
    }
    if (audio->barramento != NULL) {
        i2c_del_master_bus(audio->barramento);
    }
}

bool teste_audio_executar(void)
{
    audio_t audio = { 0 };
    uint16_t endereco = 0;
    bool ok = criar_barramento(&audio) && detectar_codec(audio.barramento, &endereco)
              && criar_i2s(&audio) && abrir_codec(&audio, endereco);
    liberar_audio(&audio);
    return ok;
}
