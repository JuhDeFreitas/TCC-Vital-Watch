# MAX30102 — Oxímetro de Pulso com ESP32-S2

Projeto de oximetria de pulso usando o sensor MAX30102 e o microcontrolador ESP32-S2-Saola-1, desenvolvido com PlatformIO + ESP-IDF 5.4.1. Calcula frequência cardíaca (FC) e saturação de oxigênio no sangue (SpO2) em tempo real via comunicação I2C.

---

## Sumário

1. [Hardware](#1-hardware)
2. [Esquema de ligação](#2-esquema-de-ligação)
3. [Configuração do sensor](#3-configuração-do-sensor)
4. [Arquitetura do software](#4-arquitetura-do-software)
5. [Algoritmo de medição](#5-algoritmo-de-medição)
6. [Visualização do sinal](#6-visualização-do-sinal)
7. [Guia de uso e montagem](#7-guia-de-uso-e-montagem)
8. [Diagnóstico de sinais](#8-diagnóstico-de-sinais)
9. [Limitações do sensor clone](#9-limitações-do-sensor-clone)
10. [Compilação e gravação](#10-compilação-e-gravação)

---

## 1. Hardware

| Componente | Especificação |
|---|---|
| Microcontrolador | ESP32-S2-Saola-1 |
| Sensor | MAX30102 (módulo breakout) |
| Framework | ESP-IDF 5.4.1 via PlatformIO |
| Alimentação sensor | 3.3 V |
| Protocolo | I2C (100 kHz) |
| Endereço I2C | 0x57 (fixo no MAX30102) |

### Sobre o MAX30102

O MAX30102 é um módulo integrado de oximetria de pulso e monitoramento de frequência cardíaca que integra:
- LED vermelho (RED) — 660 nm (LED1)
- LED infravermelho (IR) — 940 nm (LED2)
- Fotodetector de silício de alta sensibilidade
- Conversor ADC de 18 bits
- FIFO interno de 32 entradas × 6 bytes

Opera no modo **SpO2** (reflectância), onde ambos os LEDs iluminam o tecido da ponta do dedo e o fotodetector capta a luz retrodifundida. A variação do sinal acompanha o pulso cardíaco (PPG — Fotopletismografia).

---

## 2. Esquema de ligação

```
ESP32-S2-Saola-1          MAX30102 (breakout)
─────────────────          ───────────────────
GPIO 8  (SDA)  ────────── SDA
GPIO 9  (SCL)  ────────── SCL
3V3            ────────── VCC / 3.3V
GND            ────────── GND
                           INT  (não conectado — não utilizado)
```

### Resistores de pull-up

O I2C exige resistores de pull-up em SDA e SCL. **Dois cenários possíveis:**

**Módulo breakout com pull-ups embutidos (mais comum):**
A maioria dos módulos GY-MAX30102 já inclui resistores de 4,7 kΩ soldados na placa. Verifique se há resistores marcados R1/R2 no módulo — se sim, não é necessário adicionar nada.

**Conexão direta ao CI sem pull-ups externos:**
Os pull-ups internos do ESP32 (~47 kΩ) são insuficientes para 200 kHz. Por isso o projeto opera em **100 kHz**, onde os pull-ups internos funcionam de forma confiável. Para melhor robustez, adicione:

```
GPIO 8 (SDA) ──┬── 4,7 kΩ ── 3,3 V
               └── SDA do MAX30102

GPIO 9 (SCL) ──┬── 4,7 kΩ ── 3,3 V
               └── SCL do MAX30102
```

> **Por que 100 kHz e não 200 kHz?**
> Com pull-ups internos de ~47 kΩ e capacitância típica de fio (~50 pF), o tempo de subida RC é ≈ 2,35 µs. Em 200 kHz (período de bit = 5 µs), a subida ocupa 47% do período — fora da especificação I2C. Em 100 kHz (período = 10 µs), a margem é adequada.

---

## 3. Configuração do sensor

A configuração é feita via struct `max_config` em `src/main.c`. Cada campo corresponde a um registrador do MAX30102.

### Parâmetros principais

| Parâmetro | Valor | Significado |
|---|---|---|
| `MODE` | `0b011` | Modo SpO2 — LED1 (RED) + LED2 (IR) ativos |
| `SMP_AVE` | `0b010` | Média de 4 amostras por dado FIFO |
| `SPO2_SR` | `0b001` | Taxa de amostragem: 100 Hz hardware |
| `LED_PW` | `0b11` | Largura de pulso: 411 µs (ADC 18 bits) |
| `SPO2_ADC_RGE` | `0b01` | Fundo de escala ADC: 4096 nA |
| `LED1_PA` | `0x18` | Corrente LED RED: ~4,8 mA |
| `LED2_PA` | `0x18` | Corrente LED IR: ~4,8 mA |
| `FIFO_ROLLOVER_EN` | `1` | FIFO circular (não para quando cheio) |

### Taxa de amostragem efetiva

```
100 Hz (hardware) ÷ 4 (SMP_AVE) = 25 Hz efetivo
Intervalo entre amostras: 40 ms
Janela de análise: 128 amostras × 40 ms = 5,12 s
```

### Processo de ajuste da corrente dos LEDs

A corrente dos LEDs determina diretamente o nível DC do sinal ADC. A faixa ideal é **50.000 – 150.000 counts** (de um máximo de 262.143 no ADC de 18 bits). O processo de ajuste seguido foi:

| Tentativa | LED1 | LED2 | Resultado |
|---|---|---|---|
| Inicial | 0x5F (19 mA) | 0x5F (19 mA) | Saturação (262.143 em tudo) |
| Redução | 0x1F (6,2 mA) | 0x1F (6,2 mA) | Sinal fraco (≈1.150 counts) — leitura I2C incorreta mascarou |
| Aumento progressivo | 0x3F (12,5 mA) | 0x7F (25,4 mA) | Sinal aparente de 60.000/1.200 — mas I2C corrompido |
| **Após correção I2C** | 0x0F (3,0 mA) | 0x0F (3,0 mA) | Saturação com dedo (I2C correto revelou valores reais ≈260.000) |
| **Final** | 0x18 (4,8 mA) | 0x18 (4,8 mA) | IR ≈ 88.000, RED ≈ 87.000 — faixa adequada ✓ |

> **Lição aprendida:** O barramento I2C operando a 200 kHz com pull-ups internos corrompeu bytes silenciosamente, fazendo o sinal parecer muito mais fraco do que era. A redução para 100 kHz revelou os valores reais, que estavam muito acima do esperado — exigindo redução significativa da corrente.

---

## 4. Arquitetura do software

```
src/
├── main.c                  — App principal: coleta, processa, exibe
└── max30102/
    ├── i2c_api.h / .c      — Driver I2C (init, write, read)
    ├── max30102_api.h / .c — Driver MAX30102 (init, FIFO, temp)
    └── algorithm.h / .c    — Algoritmos de FC e SpO2
plotter.py                  — Gráfico em tempo real (modo diagnóstico)
```

### Correção crítica de I2C — Repeated Start

A leitura do FIFO requer uma transação I2C com **repeated start** (Sr): escreve o endereço do registrador e, sem soltar o barramento (sem STOP), inicia uma leitura. Usar duas transações separadas (STOP entre elas) faz o ponteiro interno do FIFO avançar incorretamente.

```c
// CORRETO — repeated start em transação única
i2c_master_write_read_device(I2C_PORT, MAX30102_ADDR,
                             &fifo_reg, 1,   // escreve: endereço do reg
                             buf, 6,         // lê: 6 bytes (RED + IR)
                             pdMS_TO_TICKS(1000));

// INCORRETO — duas transações com STOP entre elas
i2c_master_write_to_device(...);   // STOP
i2c_master_read_from_device(...);  // nova transação — lê registrador errado
```

### Ordem dos canais no FIFO

No modo SpO2, cada entrada do FIFO contém **6 bytes**:

```
Bytes [0..2] → LED1 = RED (660 nm)  → red_data
Bytes [3..5] → LED2 = IR  (940 nm)  → ir_data

Reconstrução de 18 bits:
value = (buf[0] << 16) | (buf[1] << 8) | buf[2];
value &= 0x3FFFF;  // máscara 18 bits
```

---

## 5. Algoritmo de medição

### Fluxo da janela de processamento

```
┌─────────────────────────────────────────────┐
│ 1. Coleta 128 amostras (5,12 s)             │
│    red_buf[i], ir_buf[i] via FIFO           │
├─────────────────────────────────────────────┤
│ 2. Verificação de presença do dedo          │
│    Se DC_IR < 10.000 → "Sem dedo"           │
├─────────────────────────────────────────────┤
│ 3. remove_dc_part()                         │
│    Subtrai a média → salva ir_mean, red_mean│
├─────────────────────────────────────────────┤
│ 4. remove_trend_line()                      │
│    Regressão linear → remove drift lento    │
├─────────────────────────────────────────────┤
│ 5. calculate_heart_rate()  ── IR             │
│    Autocorrelação → detecta período cardíaco│
├─────────────────────────────────────────────┤
│ 6. spo2_measurement()      ── IR + RED       │
│    Razão RMS/DC → fórmula Maxim             │
├─────────────────────────────────────────────┤
│ 7. Exibe FC (bpm) e SpO2 (%)               │
└─────────────────────────────────────────────┘
```

### Frequência Cardíaca — Autocorrelação

A FC é calculada pela autocorrelação do sinal IR pós-processado:

```
ACF(lag) = Σ x[i] × x[i + lag]  / N

Pico na ACF corresponde ao período cardíaco:
  lag_pico × 0,04 s = período em segundos
  FC = 60 / período
```

**Parâmetros do algoritmo:**
- `BUFFER_SIZE = 128` amostras (~5,12 s — contém ~5 ciclos a 60 bpm)
- `MINIMUM_RATIO = 0,20` — limiar mínimo de correlação normalizada
- Busca a partir de `lag > 14` (evita detectar o 2º harmônico em lag=11 para FC ≈ 68 bpm)
- Faixa detectável: 12 – 100 bpm

> **Artefato de 2ª harmônica:** O sinal cardíaco contém componentes espectrais nos múltiplos da FC fundamental (dicrotic notch, formato assimétrico). A autocorrelação pode encontrar um pico espúrio em lag = T/2, resultando em FC = 2 × valor real (ex: 136 bpm em vez de 68 bpm). A busca começando em lag > 14 suprime este artefato.

### SpO2 — Razão RMS/DC

```
R = (RMS_ir / DC_ir) / (RMS_red / DC_red)

SpO2 = −45,06 × R² + 30,354 × R + 94,845   [fórmula empírica Maxim]
```

**Condições de validade para exibir SpO2:**
| Condição | Valor mínimo | Motivo |
|---|---|---|
| `r0` (variância IR) | ≥ 500 | Sinal AC com amplitude suficiente |
| `R_ratio` | ≥ 1,20 | Relação fisiológica RED/IR válida |
| `Corr` (Pearson RED↔IR) | ≥ 0,85 | Sinal coerente entre canais |

> **Nota sobre a fórmula:** A fórmula padrão usa `R = (AC_red/DC_red) / (AC_ir/DC_ir)`. Para sensores clone com LEDs de comprimento de onda não-nominal, a relação AC entre canais pode ser invertida, exigindo ajuste. Veja a seção [Limitações do sensor clone](#9-limitações-do-sensor-clone).

---

## 6. Visualização do sinal em tempo real

O `plotter.py` exibe os canais IR e RED em um gráfico animado enquanto o firmware roda normalmente. O firmware já emite uma linha CSV a cada amostra (25 Hz), simultânea ao cálculo de FC/SpO2 — não é necessário trocar de firmware para plotar.

### Pré-requisitos

```bash
pip install pyserial matplotlib
```

### Passo a passo

1. **Grave o firmware** normalmente via PlatformIO (`Upload`).

2. **Feche o monitor serial** do PlatformIO (ou qualquer outro terminal que esteja usando a porta COM) — duas aplicações não podem abrir a mesma porta ao mesmo tempo.

3. **Abra um terminal separado** na pasta do projeto e execute:
   ```bash
   python plotter.py
   ```

4. O script lista as portas disponíveis. Se houver mais de uma, escolha o número correspondente ao ESP32:
   ```
   Portas disponíveis:
     [0] COM3  —  Silicon Labs CP210x USB to UART Bridge
     [1] COM4  —  USB Serial Device
   Escolha o número da porta: 0
   ```

5. **Coloque o dedo no sensor** — o gráfico atualiza a cada 40 ms com os valores brutos do ADC.

### O que o gráfico mostra

```
280000 ┤                                              (saturação)
       │
120000 ┤  ╭─╮  ╭─╮  ╭─╮  ← ondulação do pulso cardíaco
       │ ╯  ╰─╯  ╰─╯  ╰─╯
 80000 ┤  ← nível DC (média)
       │
     0 ┴──────────────────────────────────────────────────────
         ←──────── janela deslizante de 200 amostras (8 s) ──────────→
```

O eixo Y ajusta automaticamente ao sinal — se o valor encher a tela no máximo ou ficar raso no mínimo, ajuste `LED_PA` conforme a [tabela de estados DC](#8-diagnóstico-de-sinais).

### Formato CSV emitido pelo firmware

A cada amostra coletada, `main.c` imprime:
```
IR:88421,RED:89103
IR:88350,RED:89050
...
```
Ao fim de cada janela de 128 amostras (a cada ~5 s), imprime os resultados do algoritmo:
```
=== Resultado ===
  FC  : 72 bpm
  SpO2: 96.3 %
  ...
```
O `plotter.py` ignora as linhas que não começam com `IR:`, portanto os dois outputs coexistem sem interferência.

### Interpretação do gráfico

| Característica | Sinal adequado | Problema |
|---|---|---|
| Nível DC (com dedo) | 50.000 – 150.000 counts | Abaixo: corrente baixa; Acima: saturação |
| Linha reta no máximo (262.143) | — | Saturação: reduzir `LED_PA` |
| Linha reta em ~1.000 | — | Sem dedo, ou corrente muito baixa |
| Ondulação rítmica visível | Pulsos cardíacos claros | Sem ondulação: pressão incorreta |
| IR e RED no mesmo nível | — | Canais cruzados ou sem dedo |

---

## 7. Guia de uso e montagem

### Montagem física

1. Conecte o módulo MAX30102 ao ESP32-S2 conforme o [esquema](#2-esquema-de-ligação)
2. Verifique se o módulo já possui pull-ups (resistores 4,7 kΩ) — a maioria dos breakouts tem
3. Alimente via USB (3,3 V disponível no pino 3V3 do Saola-1)

### Como posicionar o dedo

O sensor opera em **modo reflectância** — a luz atravessa a pele e retorna ao fotodetector.

```
Vista superior do sensor (janela óptica):

  ┌──────────────────────────────┐
  │  [LED1 RED] [LED2 IR] [PD]  │  ← posição dos componentes
  └──────────────────────────────┘
         ↑
    Apoia aqui a ponta do dedo indicador

Pressão ideal:
  - Moderada e constante (não pressionar demais — comprime capilares)
  - Dedo reto, não inclinado (maximiza cobertura dos 3 componentes)
  - Aguardar 3–5 s parado antes de ler o resultado
```

**Erros comuns de posicionamento:**

| Sintoma | Causa provável |
|---|---|
| Linha reta no gráfico com dedo | Pressão excessiva (vasos comprimidos) ou saturação (LED muito forte) |
| IR e RED iguais e baixos | Sem dedo ou contato muito fraco |
| Sinal oscilando muito | Movimento do dedo ou pressão irregular |
| FC = 2× o valor esperado | Harmônico detectado (corrigido pelo algoritmo) |

### Saída serial esperada

```
=== Resultado ===
  FC  : 72 bpm
  SpO2: 96.3 %
  DC  : IR=88500  RED=89200  (ideal: RED>=IR)
  AC  : IR_RMS=420.5  RED_RMS=630.2
  R   : 1.503  (ideal: >=1.40 para SpO2 > 95%)
  Corr: 0.951  r0=176820.2
```

**Campos de diagnóstico:**

| Campo | Descrição |
|---|---|
| `DC` | Componente DC (nível médio) — reflexo da intensidade luminosa |
| `AC IR/RED_RMS` | Amplitude do sinal pulsátil (componente AC após remoção DC) |
| `R` | Razão normalizada dos canais (> 1,40 para SpO2 válido) |
| `Corr` | Correlação de Pearson RED↔IR (> 0,85 indica sinal coerente) |
| `r0` | Variância do sinal IR (proporcional ao quadrado da amplitude AC) |

---

## 8. Diagnóstico de sinais

### Tabela de estados DC

| Valor DC (com dedo) | Diagnóstico | Ação |
|---|---|---|
| < 10.000 | Sem dedo ou contato muito fraco | Reposicionar dedo |
| 10.000 – 50.000 | Sinal fraco | Aumentar `LED_PA` em 0x04 |
| **50.000 – 150.000** | **Faixa ideal** ✓ | — |
| 150.000 – 260.000 | Sinal forte, próximo da saturação | Reduzir `LED_PA` em 0x04 |
| 262.143 | Saturado (ADC no máximo) | Reduzir `LED_PA` em 0x08 |

### Fórmula de corrente dos LEDs

```
Corrente (mA) ≈ LED_PA × 0,20 mA

Exemplos:
  0x0F = 15 → 3,0 mA
  0x18 = 24 → 4,8 mA  ← configuração atual
  0x3F = 63 → 12,6 mA
  0x7F = 127 → 25,4 mA
  0xFF = 255 → 51,0 mA (máximo)
```

### Ajuste iterativo recomendado

```
1. Grave o firmware com plotter.py (modo CSV)
2. Coloque o dedo e observe o DC
3. Se DC > 200.000 → reduza LED_PA em 0x08, regrave
4. Se DC < 20.000  → aumente LED_PA em 0x08, regrave
5. Repita até DC ficar em 60.000 – 120.000
6. Verifique se há ondulação cardíaca visível no RED
7. Grave o firmware com algoritmo completo
```

---

## 9. Limitações do sensor clone

Módulos MAX30102 de baixo custo (comuns em marketplaces) frequentemente apresentam desvios em relação ao sensor genuíno Maxim:

### Relação AC invertida

Em um sensor genuíno com LEDs em 660 nm e 940 nm exatos, a oxyHb absorve ~4× mais IR (940 nm, ε = 1,214 cm⁻¹/mM) do que RED (660 nm, ε = 0,319 cm⁻¹/mM). Isso significa que **o canal IR deve ter maior variação AC relativa** ao DC, resultando em:

```
R_genuíno = (AC_red/DC_red) / (AC_ir/DC_ir) ≈ 0,50 – 0,70  para SpO2 95–100%
```

No sensor clone deste projeto, observou-se a relação **invertida** (`AC_red > AC_ir`), típica de LEDs com comprimento de onda ligeiramente desviado do nominal. A correção aplicada foi inverter a fórmula de R:

```c
// Fórmula corrigida para o clone
double R = (ir_rms / (double)ir_mean) / (red_rms / (double)red_mean);
```

Isso faz com que o R utilizado na fórmula Maxim fique na faixa 0,50–0,70 para leituras fisiologicamente plausíveis.

### Impacto na precisão

| Parâmetro | Sensor genuíno | Clone (este projeto) |
|---|---|---|
| FC | ±2 bpm (típico) | ±5–8 bpm (variável) |
| SpO2 | ±2% (calibrado) | ±3–5% (estimativa não calibrada) |
| SpO2 absoluta | Confiável | Indica tendência — não substitui oxímetro clínico |

> Para aplicações clínicas ou de pesquisa, utilize sensores MAX30102 originais e realize calibração empírica comparando com oxímetro de referência certificado.

---

## 10. Compilação e gravação

### Pré-requisitos

- [Visual Studio Code](https://code.visualstudio.com/)
- Extensão [PlatformIO IDE](https://platformio.org/install/ide?install=vscode)
- Driver USB-Serial para ESP32-S2 (CP2102 ou CH340, dependendo do módulo)

### Compilar e gravar

```bash
# Via terminal PlatformIO
pio run --target upload

# Via VS Code: botão "→ Upload" na barra inferior
```

### Monitor serial

```bash
pio device monitor --baud 115200

# Via VS Code: botão "🔌 Monitor" na barra inferior
```

### Estrutura de arquivos relevantes

```
Max30102/
├── platformio.ini          — Configuração da plataforma (ESP-IDF, board)
├── plotter.py              — Visualizador de sinal em tempo real (Python)
└── src/
    ├── main.c              — Aplicação principal
    └── max30102/
        ├── algorithm.c/h   — FC (autocorrelação) e SpO2 (RMS ratio)
        ├── i2c_api.c/h     — Driver I2C
        └── max30102_api.c/h — Driver do sensor (registradores, FIFO)
```

---

## Referências

- [MAX30102 Datasheet — Maxim Integrated](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX30102.pdf)
- [MAXREFDES117 — Reference Design Maxim (algoritmo SpO2)](https://www.analog.com/en/resources/reference-designs/maxrefdes117.html)
- [ESP-IDF I2C Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s2/api-reference/peripherals/i2c.html)
- Coeficientes de extinção da hemoglobina: Scott Prahl, Oregon Medical Laser Center
