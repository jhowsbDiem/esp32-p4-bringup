# ESP32-P4 Bringup — firmware de teste da ESP32-P4-Module-DEV-KIT 🔬

[![Hardware](https://img.shields.io/badge/Hardware-ESP32--P4-blue.svg)](https://www.espressif.com/en/products/socs/esp32-p4)
[![Firmware](https://img.shields.io/badge/Firmware-ESP--IDF%20v6.1-green.svg)](https://docs.espressif.com/projects/esp-idf/)

ESP32-P4 Bringup é um firmware em ESP-IDF para a placa Waveshare
ESP32-P4-Module-DEV-KIT que, a cada boot, executa uma sequência de testes de
hardware e imprime na serial o resultado de cada um (OK/FALHA) e uma linha de
resumo. Serve para confirmar que a placa é a esperada, medir o desempenho da
memória e testar o caminho de áudio de ponta a ponta, do codec ao microfone.

---

## 🚀 Funcionalidades

* **Diagnóstico do chip:** modelo, núcleos, revisão, recursos, tamanho da flash, versão do ESP-IDF e heap interno livre.
* **Conferência da flash:** falha quando o tamanho físico da flash difere do tamanho configurado no binário.
* **Detecção da PSRAM:** confirma a PSRAM de 32 MB inicializada e informa a memória livre e o maior bloco alocável.
* **Velocidade da memória:** escrita, leitura e cópia em MB/s, na PSRAM e na RAM interna.
* **Teste de áudio:** encontra o codec ES8311 no I2C, toca um tom de 1 kHz por 1 s no alto-falante e confere pelo microfone da placa que o tom foi captado.
* **Resumo:** uma linha final com o resultado de todos os testes.

---

## 🛠️ Especificações de Hardware

* **Placa:** Waveshare ESP32-P4-Module-DEV-KIT.
* **SoC:** ESP32-P4NRW32, revisão v1.3 (RISC-V dual-core a 360 MHz, 32 MB de PSRAM integrada a 200 MHz).
* **Flash:** 16 MB, GigaDevice (SPI, modo DIO).
* **Console:** ponte USB-UART CH343 na USB-C, a 115200 baud.
* **Áudio:** codec ES8311 no I2C (endereço 0x18), amplificador NS4150B, alto-falante no conector da placa e microfone na entrada do codec.

| Sinal | GPIO |
|:---|:---:|
| I2C SDA / SCL | 7 / 8 |
| I2S MCLK / BCLK / WS | 13 / 12 / 10 |
| I2S DOUT (P4 → codec) / DIN (codec → P4) | 9 / 11 |
| PA_CTRL (liga o amplificador) | 53 |

---

## 📁 Estrutura de Pastas

```
.
├── CMakeLists.txt          # projeto ESP-IDF, com MINIMAL_BUILD
├── sdkconfig.defaults      # target, revisão do chip, flash e PSRAM
├── dependencies.lock       # versões fixadas dos componentes do registro
├── main/
│   └── main.c              # tabela de testes e linha de resumo
└── components/
    ├── teste_chip/         # chip, flash, versão do ESP-IDF e heap interno
    ├── teste_psram/        # detecção e velocidade da PSRAM
    └── teste_audio/        # codec, tom no alto-falante e conferência pelo microfone
```

---

## 📚 Dependências de Software

* `ESP-IDF v6.1` (framework, toolchain RISC-V e drivers de flash, PSRAM, timer, I2C e I2S)
* `espressif/esp_codec_dev` 1.6.2, do ESP Component Registry (controle do ES8311). O primeiro build baixa o componente para `managed_components/` e precisa de internet; a versão fica fixada no `dependencies.lock`.

---

## 🎛️ Uso

Os testes rodam a cada boot, na ordem da tabela:

| Teste | O que mostra | FALHA quando |
|:---:|:---|:---|
| `chip` | modelo, núcleos, revisão, recursos, flash, versão do ESP-IDF e heap interno | o tamanho físico da flash difere do configurado |
| `psram` | tamanho da PSRAM, memória livre e maior bloco livre | a PSRAM não é inicializada ou não tem 32 MB |
| `psram_vel` | escrita, leitura e cópia em MB/s, na PSRAM e na RAM interna | uma alocação falha ou a cópia não confere |
| `audio` | endereço do codec e níveis do microfone em 1 kHz: silêncio, tom e relação | o codec não responde no I2C, a abertura do codec ou do I2S falha, ou o tom fica menos de 20 dB acima do silêncio |

Exemplo da saída na placa:

```
I (..) bringup: Firmware de teste da ESP32-P4-Module-DEV-KIT iniciado
I (..) teste_chip: Modelo: esp32p4 | núcleos: 2 | revisão: v1.3
I (..) teste_chip: Recursos: nenhum
I (..) teste_chip: Flash: física 16 MB | configurada 16 MB
I (..) teste_chip: ESP-IDF: v6.1
I (..) teste_chip: Heap interno livre: 597219 bytes
I (..) bringup: chip: OK
I (..) teste_psram: PSRAM: 32 MB | livre no heap: 32765 KB | maior bloco livre: 32256 KB
I (..) bringup: psram: OK
I (..) teste_psram_vel: PSRAM (4096 KB): escrita 82 MB/s | leitura 77 MB/s | cópia 55 MB/s
I (..) teste_psram_vel: RAM interna (128 KB): escrita 645 MB/s | leitura 161 MB/s | cópia 355 MB/s
I (..) teste_psram_vel: Tempo total: 783 ms
I (..) bringup: psram_vel: OK
I (..) teste_audio: Codec ES8311 encontrado em 0x18 (SDA GPIO7, SCL GPIO8)
...                                   (logs do esp_codec_dev)
I (..) teste_audio: Codec aberto: 16000 Hz, 16 bits, 2 canais
I (..) teste_audio: Tocando tom de 1000 Hz por 1000 ms (volume 90)
I (..) teste_audio: Microfone em 1000 Hz: silêncio -97 dBFS | tom -57 dBFS | relação 40 dB (mínimo 20 dB)
I (..) bringup: audio: OK
I (..) bringup: Resumo: chip=OK psram=OK psram_vel=OK audio=OK
```

A linha `Resumo` lista cada teste como `nome=OK` ou `nome=FALHA`. Ela sai em
vermelho (nível de erro) quando pelo menos um teste falha.

O teste de áudio leva cerca de 2 s e apita a cada boot. Entre os logs do
`esp_codec_dev` aparece o aviso `W ... the rx channel on I2S0 is switched from
master to slave for full-duplex mode`, que é esperado: o TX e o RX compartilham
os pinos de clock, e o RX passa a usar o clock do TX.

---

## ⚙️ Configuração e Instalação

1. **Clone o repositório:**
   ```bash
   git clone https://github.com/jhowsbDiem/esp32-p4-bringup.git
   cd esp32-p4-bringup
   ```
2. **Instale o ESP-IDF v6.1** seguindo o [guia oficial da Espressif](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32p4/get-started/index.html),
   que cobre Windows, Linux e macOS. Os comandos dos passos seguintes rodam num terminal com o
   ambiente do ESP-IDF ativo.
3. **Revisão do chip:** o `sdkconfig.defaults` configura o projeto para ESP32-P4 de revisão v1.x
   (`CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`). O ESP-IDF v6.1 compila por padrão só para chips
   v3.x, e o bootloader de um chip v1.x recusa esse binário. As duas linhas de revisão são
   incompatíveis entre si: numa placa com `ESP32-P4NRW32X` (v3.x), remova essa opção e apague o
   `sdkconfig` antes de compilar.
4. **Ligue o alto-falante** no conector da placa; sem ele o teste de áudio dá FALHA. Depois **conecte a placa**
   pela USB-C da ponte serial. A porta aparece com um nome que depende do sistema:
   * **Windows:** `COM3`, `COM4`... (veja no Gerenciador de Dispositivos).
   * **Linux:** `/dev/ttyACM0`.
   * **macOS:** `/dev/cu.usbmodem...`.
5. **Compile, grave e abra o monitor**, trocando `<PORTA>` pelo nome do passo anterior:
   ```bash
   idf.py build
   idf.py -p <PORTA> flash monitor
   ```
   O primeiro build baixa o `esp_codec_dev` e precisa de internet. Para sair do monitor, use `Ctrl+]`; no
   teclado ABNT2, use `Ctrl+T` seguido de `Ctrl+X`.

O `sdkconfig.defaults` só é aplicado às opções que ainda não existem no `sdkconfig`. Depois de
editá-lo, apague o `sdkconfig` para que a mudança valha no próximo build.

---

## 🧠 Arquitetura

* **Um componente por área testada:** cada teste é uma função `bool <nome>_executar(void)` num componente em `components/`; o `main` só percorre uma tabela de testes, e um teste novo entra como uma linha nessa tabela.
* **`MINIMAL_BUILD`:** o build inclui só o `main` e o que ele declara, então cada componente lista as suas dependências em `PRIV_REQUIRES`.
* **Configuração versionada só no `sdkconfig.defaults`:** o `sdkconfig` é gerado pelo build e fica fora do git; um clone novo compila sem `idf.py set-target`.
* **Integridade da PSRAM no boot:** com `CONFIG_SPIRAM_MEMTEST`, o ESP-IDF testa os 32 MB antes do app e aborta o boot se houver falha; o firmware mede só a velocidade.
* **Bloco de 4 MB na medição da PSRAM:** bem maior que o cache L2 de 128 KB, para que o resultado reflita a PSRAM e não o cache.
* **Microfone medido só em 1 kHz:** o algoritmo de Goertzel, aplicado a cada bloco de 10 ms, e a mediana dos blocos deixam de fora o ruído das outras frequências, a deriva do ADC e ruídos em rajada, como voz e palmas.
* **Só o canal esquerdo do I2S RX:** o ES8311 põe no canal direito uma cópia interna do DAC, que mostraria o tom mesmo sem alto-falante.
* **700 ms de espera antes de medir o silêncio:** o ADC do ES8311 leva cerca de 600 ms para acomodar depois de ligar.
* **100 ms de silêncio depois do tom:** empurram o fim do tom para fora da fila do DMA antes de o amplificador desligar.

---

## 📊 Estado Atual do Projeto

### ✅ Funcionando (Validado em Bancada)
* **Boot no chip v1.3:** o binário aceita revisões de v0.0 a v1.99 e dá boot sem loop de reset.
* **Diagnóstico do chip:** informa chip v1.3 com 2 núcleos e flash de 16 MB, e acusa FALHA com a flash configurada em 8 MB.
* **PSRAM:** 32 MB detectados a 200 MHz, com o teste de memória do boot aprovado.
* **Velocidade da memória:** a PSRAM fica de 6 a 8 vezes mais lenta que a RAM interna em escrita e cópia.
* **Áudio:** codec em 0x18 e tom de 1 kHz audível no alto-falante. O microfone mede o tom 40 dB acima do silêncio. Sem alto-falante, a relação fica perto de 0 dB e chega a 10 dB com voz e palmas perto da placa, sempre abaixo do limiar de 20 dB.

### 🗺️ Próximos Passos Planejados
* **Eco de áudio:** o som do microfone sai no alto-falante em tempo real.
* **GPIO:** LED e botão pelo header de 40 pinos.
* **microSD:** leitura e escrita no cartão.
* **Wi-Fi 6 e BLE:** pelo ESP32-C6, via `esp_hosted`.
* **Câmera:** captura pela MIPI-CSI com a RPi Camera (B).

---

## ✒️ Autores e Licença

* **Desenvolvedor:** Jonathan dos Santos Bezerra.
* **Licença:** MIT — veja [LICENSE](LICENSE).
