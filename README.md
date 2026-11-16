# ESP32-P4 Bringup — firmware de teste da ESP32-P4-Module-DEV-KIT 🔬

[![Hardware](https://img.shields.io/badge/Hardware-ESP32--P4-blue.svg)](https://www.espressif.com/en/products/socs/esp32-p4)
[![Firmware](https://img.shields.io/badge/Firmware-ESP--IDF%20v6.1-green.svg)](https://docs.espressif.com/projects/esp-idf/)

ESP32-P4 Bringup é um firmware em ESP-IDF para a placa Waveshare
ESP32-P4-Module-DEV-KIT que, a cada boot, executa uma sequência de testes de
hardware e imprime na serial o resultado de cada um (OK/FALHA) e uma linha de
resumo. Serve para confirmar que a placa é a esperada e para medir o desempenho
da memória antes de usar os periféricos.

---

## 🚀 Funcionalidades

* **Diagnóstico do chip:** modelo, núcleos, revisão, recursos, tamanho da flash, versão do ESP-IDF e heap interno livre.
* **Conferência da flash:** falha quando o tamanho físico da flash difere do tamanho configurado no binário.
* **Detecção da PSRAM:** confirma a PSRAM de 32 MB inicializada e informa a memória livre e o maior bloco alocável.
* **Velocidade da memória:** escrita, leitura e cópia em MB/s, na PSRAM e na RAM interna.
* **Resumo:** uma linha final com o resultado de todos os testes.

---

## 🛠️ Especificações de Hardware

* **Placa:** Waveshare ESP32-P4-Module-DEV-KIT.
* **SoC:** ESP32-P4NRW32, revisão v1.3 (RISC-V dual-core a 360 MHz, 32 MB de PSRAM integrada a 200 MHz).
* **Flash:** 16 MB, GigaDevice (SPI, modo DIO).
* **Console:** ponte USB-UART CH343 na USB-C, a 115200 baud.

---

## 📁 Estrutura de Pastas

```
.
├── CMakeLists.txt          # projeto ESP-IDF, com MINIMAL_BUILD
├── sdkconfig.defaults      # target, revisão do chip, flash e PSRAM
├── main/
│   └── main.c              # tabela de testes e linha de resumo
└── components/
    ├── teste_chip/         # chip, flash, versão do ESP-IDF e heap interno
    └── teste_psram/        # detecção e velocidade da PSRAM
```

---

## 📚 Dependências de Software

* `ESP-IDF v6.1` (framework, toolchain RISC-V e drivers de flash, PSRAM e timer)

---

## 🎛️ Uso

Os testes rodam a cada boot, na ordem da tabela:

| Teste | O que mostra | FALHA quando |
|:---:|:---|:---|
| `chip` | modelo, núcleos, revisão, recursos, flash, versão do ESP-IDF e heap interno | o tamanho físico da flash difere do configurado |
| `psram` | tamanho da PSRAM, memória livre e maior bloco livre | a PSRAM não é inicializada ou não tem 32 MB |
| `psram_vel` | escrita, leitura e cópia em MB/s, na PSRAM e na RAM interna | uma alocação falha ou a cópia não confere |

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
I (..) bringup: Resumo: chip=OK psram=OK psram_vel=OK
```

A linha `Resumo` lista cada teste como `nome=OK` ou `nome=FALHA`. Ela sai em
vermelho (nível de erro) quando pelo menos um teste falha.

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
4. **Conecte a placa** pela USB-C da ponte serial. A porta aparece com um nome que depende do sistema:
   * **Windows:** `COM3`, `COM4`... (veja no Gerenciador de Dispositivos).
   * **Linux:** `/dev/ttyACM0`.
   * **macOS:** `/dev/cu.usbmodem...`.
5. **Compile, grave e abra o monitor**, trocando `<PORTA>` pelo nome do passo anterior:
   ```bash
   idf.py build
   idf.py -p <PORTA> flash monitor
   ```
   Para sair do monitor, use `Ctrl+]`; no teclado ABNT2, use `Ctrl+T` seguido de `Ctrl+X`.

O `sdkconfig.defaults` só é aplicado às opções que ainda não existem no `sdkconfig`. Depois de
editá-lo, apague o `sdkconfig` para que a mudança valha no próximo build.

---

## 🧠 Arquitetura

* **Um componente por área testada:** cada teste é uma função `bool <nome>_executar(void)` num componente em `components/`; o `main` só percorre uma tabela de testes, e um teste novo entra como uma linha nessa tabela.
* **`MINIMAL_BUILD`:** o build inclui só o `main` e o que ele declara, então cada componente lista as suas dependências em `PRIV_REQUIRES`.
* **Configuração versionada só no `sdkconfig.defaults`:** o `sdkconfig` é gerado pelo build e fica fora do git; um clone novo compila sem `idf.py set-target`.
* **Integridade da PSRAM no boot:** com `CONFIG_SPIRAM_MEMTEST`, o ESP-IDF testa os 32 MB antes do app e aborta o boot se houver falha; o firmware mede só a velocidade.
* **Bloco de 4 MB na medição da PSRAM:** bem maior que o cache L2 de 128 KB, para que o resultado reflita a PSRAM e não o cache.

---

## 📊 Estado Atual do Projeto

### ✅ Funcionando (Validado em Bancada)
* **Boot no chip v1.3:** o binário aceita revisões de v0.0 a v1.99 e dá boot sem loop de reset.
* **Diagnóstico do chip:** informa chip v1.3 com 2 núcleos e flash de 16 MB, e acusa FALHA com a flash configurada em 8 MB.
* **PSRAM:** 32 MB detectados a 200 MHz, com o teste de memória do boot aprovado.
* **Velocidade da memória:** a PSRAM fica de 6 a 8 vezes mais lenta que a RAM interna em escrita e cópia.

### 🗺️ Próximos Passos Planejados
* **Áudio:** tom gerado por software no alto-falante, pelo codec e pelo amplificador da placa.
* **GPIO:** LED e botão pelo header de 40 pinos.
* **microSD:** leitura e escrita no cartão.
* **Wi-Fi 6 e BLE:** pelo ESP32-C6, via `esp_hosted`.
* **Câmera:** captura pela MIPI-CSI com a RPi Camera (B).

---

## ✒️ Autores e Licença

* **Desenvolvedor:** Jonathan dos Santos Bezerra.
* **Licença:** MIT — veja [LICENSE](LICENSE).
