# MAX30102 — Oxímetro de Pulso (VitalWatch)

Módulo de oximetria de pulso do projeto **VitalWatch**, sistema de monitoramento hospitalar IoT. Implementado no ESP32-S2-Saola-1 com ESP-IDF 5.4.1 via PlatformIO. Calcula **frequência cardíaca (FC)** e **saturação de oxigênio (SpO2)** em tempo real via I2C.

---

## Sumário

1. [Hardware e ligação](#1-hardware-e-ligação)
2. [Configuração do sensor](#2-configuração-do-sensor)
3. [Arquitetura do software](#3-arquitetura-do-software)
4. [Algoritmo de medição](#4-algoritmo-de-medição)
5. [Visualização em tempo real](#5-visualização-em-tempo-real)
6. [Testes automatizados](#6-testes-automatizados)
7. [Limitações do módulo clone](#7-limitações-do-módulo-clone)
8. [Compilação e uso](#8-compilação-e-uso)
9. [Referências](#9-referências)

---

## 1. Hardware e ligação

| Componente | Especificação |
|---|---|
| Microcontrolador | ESP32-S2-Saola-1 |
| Sensor | MAX30102 (módulo breakout) |
| Protocolo | I2C · 100 kHz · endereço 0x57 |
| Alimentação | 3,3 V |

```
ESP32-S2-Saola-1       MAX30102
────────────────       ────────
GPIO 8 (SDA) ──────── SDA
GPIO 9 (SCL) ──────── SCL
3V3          ──────── VCC
GND          ──────── GND
```

> **Por que 100 kHz?** Os pull-ups internos do ESP32 (~47 kΩ) são insuficientes para 200 kHz — o tempo de subida RC excede a especificação I2C. A maioria dos módulos breakout já inclui pull-ups de 4,7 kΩ soldados; se não tiver, adicione externamente.

---

## 2. Configuração do sensor

Configuração em `src/main.c` via struct `max_config`:

| Registrador | Valor | Efeito |
|---|---|---|
| `MODE` | `0b011` | Modo SpO2 — LED RED + IR ativos |
| `SMP_AVE` | `0b010` | Média de 4 amostras → **25 Hz efetivo** |
| `SPO2_SR` | `0b001` | 100 Hz hardware |
| `LED_PW` | `0b11` | Pulso 411 µs — ADC **18 bits** |
| `SPO2_ADC_RGE` | `0b01` | Fundo de escala 4096 nA |
| `LED1_PA / LED2_PA` | `0x18` | Corrente ~4,8 mA → DC ≈ 88–89 k counts |

```
Taxa efetiva: 100 Hz ÷ 4 (SMP_AVE) = 25 Hz
Janela de análise: 128 amostras × 40 ms = 5,12 s
```

A faixa-alvo de DC é **50.000–150.000 counts** (ADC máximo = 262.143). O valor `0x18` foi determinado experimentalmente para este módulo específico.

---

## 3. Arquitetura do software

```
src/
├── main.c                  — Coleta, filtra, exibe FC e SpO2
└── max30102/
    ├── i2c_api.h/c         — Driver I2C (init, write, read)
    ├── max30102_api.h/c    — Driver MAX30102 (init, FIFO, temp)
    └── algorithm.h/c       — Algoritmos de FC e SpO2
plotter.py                  — Gráfico em tempo real (Python)
test/
├── test_algorithm/         — Testes nativos (PC)
└── test_sensor/            — Testes embarcados (ESP32)
```

**Leitura do FIFO — Repeated Start obrigatório:**
O endereço do registrador FIFO deve ser enviado na mesma transação I2C que a leitura (sem STOP intermediário). O ESP-IDF disponibiliza `i2c_master_write_read_device` para isso. Usar duas transações separadas corrompe o ponteiro interno do FIFO.

---

## 4. Algoritmo de medição

```
Coleta 128 amostras (5,12 s)
        │
        ▼
remove_dc_part()       — subtrai a média; guarda ir_mean, red_mean
        │
        ▼
remove_trend_line()    — regressão linear; remove drift lento
        │
   ┌────┴────┐
   ▼         ▼
calculate_  spo2_
heart_rate  measurement
(IR)        (IR + RED)
```

### Frequência Cardíaca — Autocorrelação

```
ACF(lag) = Σ x[i] × x[i + lag] / N
FC = 60 / (lag_pico × 0,04 s)
```

- Busca começa em `lag > 14` para suprimir o 2° harmônico (~68 bpm detectado como ~136 bpm)
- Faixa detectável: 12–100 bpm

### SpO2 — Razão RMS/DC

```
R = (RMS_ir / DC_ir) / (RMS_red / DC_red)
SpO2 = −45,06 × R² + 30,354 × R + 94,845   [fórmula Maxim]
```

SpO2 é exibido apenas quando: `r0 ≥ 500` · `R_ratio ≥ 1,20` · `Correlação RED↔IR ≥ 0,85`.

---

## 5. Visualização em tempo real

O firmware emite uma linha CSV a cada amostra, simultânea ao cálculo de FC/SpO2:

```
IR:88421,RED:89103
IR:88350,RED:89050
...
=== Resultado ===
  FC  : 72 bpm
  SpO2: 96.3 %
```

Para visualizar graficamente:

```bash
pip install pyserial matplotlib
python plotter.py
```

Feche o monitor serial do PlatformIO antes de executar o script — a mesma porta COM não pode ser aberta por dois processos.

---

## 6. Testes automatizados

```bash
pio test -e native                # Lógica + simulação — sem hardware
pio test -e esp32-s2-sensor-test  # Hardware real — ESP32 + sensor conectado
```

### Testes nativos (16 casos — rodam no PC)

**Lógica básica (10):** verificam cada função do algoritmo de forma isolada.

| Teste | Verifica |
|---|---|
| `test_rms_sinal_zero_retorna_zero` | RMS de buffer zerado = 0 |
| `test_rms_sinal_constante` | RMS de constante = o próprio valor |
| `test_remove_dc_retorna_media_correta` | Médias IR e RED retornadas corretamente |
| `test_remove_dc_zera_sinal_constante` | Buffer constante → zero após remover DC |
| `test_detrend_remove_rampa` | Rampa linear → zero após `remove_trend_line` |
| `test_correlacao_sinais_identicos_igual_1` | Correlação de sinal consigo mesmo = +1 |
| `test_correlacao_sinais_opostos_igual_menos1` | Correlação de sinal invertido = −1 |
| `test_fc_sinal_zero_retorna_negativo` | FC retorna −1 para sinal nulo |
| `test_fc_senoide_60bpm_detectada` | Detecta 60 bpm em senoide com período 25 amostras |
| `test_spo2_medias_zero_retorna_negativo` | SpO2 retorna −1 quando médias = 0 |

**Simulação PPG (6):** passam sinais sintéticos (DC ≈ 88–89 k, AC ≈ 2–3 k counts) pelo pipeline completo.

| Teste | Verifica |
|---|---|
| `test_simulacao_sinal_tipico_60bpm` | FC detectada em ±10 bpm para sinal a 60 bpm |
| `test_simulacao_sinal_tipico_75bpm` | FC detectada em ±10 bpm para sinal a 75 bpm |
| `test_simulacao_spo2_faixa_fisiologica` | SpO2 entre 85–100 % com relação AC do clone |
| `test_simulacao_sem_dedo_dc_baixo` | DC IR < 10.000 identificado como "sem dedo" |
| `test_simulacao_saturacao_dc_alto` | Valores = 262.143 identificam saturação |
| `test_simulacao_dc_na_faixa_ideal` | DC ≈ 88 k confirmado dentro da faixa 50–150 k |

### Testes embarcados (9 casos — requerem ESP32 + sensor)

| Teste | Verifica |
|---|---|
| `test_i2c_inicializa_sem_erro` | Sensor responde no endereço 0x57 |
| `test_part_id_correto` | REG_PART_ID = 0x15 |
| `test_rev_id_nao_zero` | REG_REV_ID ≠ 0x00 |
| `test_escreve_le_corrente_led1/2` | Escrita e leitura de LED_PA consistentes |
| `test_reset_limpa_fifo_pointers` | WR_PTR e RD_PTR = 0 após reset |
| `test_fifo_valores_dentro_de_18_bits` | RED e IR ≥ 0 e ≤ 262.143 |
| `test_fifo_multiplas_leituras_consecutivas` | 5 leituras válidas consecutivas |
| `test_temperatura_plausivel` | Temperatura interna entre 10 °C e 55 °C |

---

## 7. Limitações do módulo clone

Módulos MAX30102 de baixo custo frequentemente possuem LEDs com comprimento de onda desviado do nominal, alterando a relação AC entre canais IR e RED. Neste projeto, o canal RED apresentou maior variação AC que o IR (comportamento inverso ao sensor original), exigindo a inversão da fórmula de R:

```c
// Correção aplicada para o clone
R = (ir_rms / ir_mean) / (red_rms / red_mean);
```

O sensor também apresenta **sensibilidade significativa à movimentação**: pequenos deslocamentos do dedo degradam o sinal fotopletismográfico e resultam em leituras rejeitadas pelo filtro de qualidade. Para aplicações clínicas, utilize sensor MAX30102 original com calibração empírica comparativa.

---

## 8. Compilação e uso

```bash
# Compilar e gravar
pio run --target upload

# Monitor serial
pio device monitor --baud 115200

# Testes sem hardware
pio test -e native

# Testes com hardware
pio test -e esp32-s2-sensor-test
```

**Posicionamento do dedo:** apoiar a ponta do dedo indicador sobre a janela óptica com pressão moderada e constante. Aguardar 5–6 s parado para que o algoritmo complete uma janela de análise (128 amostras × 40 ms).

---

## 9. Referências

- [MAX30102 Datasheet — Maxim/Analog Devices](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX30102.pdf)
- [MAXREFDES117 — Algoritmo SpO2 de referência Maxim](https://www.analog.com/en/resources/reference-designs/maxrefdes117.html)
- [ESP-IDF I2C Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-reference/peripherals/i2c.html)
- Coeficientes de extinção da hemoglobina: Scott Prahl, Oregon Medical Laser Center
