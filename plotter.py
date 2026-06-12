import serial
import serial.tools.list_ports
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

# ── Configuração ──────────────────────────────────────────────
BAUD     = 115200
JANELA   = 200   # quantas amostras mostrar no gráfico de uma vez
# ─────────────────────────────────────────────────────────────

def escolher_porta():
    portas = list(serial.tools.list_ports.comports())
    if not portas:
        print("Nenhuma porta serial encontrada.")
        exit(1)
    if len(portas) == 1:
        print(f"Usando porta: {portas[0].device}")
        return portas[0].device
    print("Portas disponíveis:")
    for i, p in enumerate(portas):
        print(f"  [{i}] {p.device}  —  {p.description}")
    idx = int(input("Escolha o número da porta: "))
    return portas[idx].device

porta = escolher_porta()
ser   = serial.Serial(porta, BAUD, timeout=1)

ir_buf  = deque([0] * JANELA, maxlen=JANELA)
red_buf = deque([0] * JANELA, maxlen=JANELA)

fig, ax = plt.subplots(figsize=(12, 5))
fig.suptitle("MAX30102 — Sinal PPG em tempo real", fontsize=13)

line_ir,  = ax.plot([], [], label="IR",  color="tab:blue",   lw=1.2)
line_red, = ax.plot([], [], label="RED", color="tab:red",    lw=1.2)

ax.set_xlim(0, JANELA)
ax.set_ylim(0, 280000)
ax.set_xlabel("Amostras (25 Hz)")
ax.set_ylabel("Contagens ADC (18-bit)")
ax.legend(loc="upper right")
ax.grid(True, alpha=0.3)

xs = list(range(JANELA))

def atualizar(_):
    # Drena todas as linhas disponíveis na serial de uma vez
    while ser.in_waiting:
        try:
            linha = ser.readline().decode("utf-8", errors="ignore").strip()
            # Formato esperado: IR:107234,RED:108902
            if linha.startswith("IR:"):
                partes = linha.split(",")
                ir  = int(partes[0].split(":")[1])
                red = int(partes[1].split(":")[1])
                ir_buf.append(ir)
                red_buf.append(red)
        except (ValueError, IndexError):
            pass

    line_ir.set_data(xs, list(ir_buf))
    line_red.set_data(xs, list(red_buf))

    # Ajusta eixo Y automaticamente ao sinal atual
    todos = list(ir_buf) + list(red_buf)
    ymin = max(0, min(todos) - 2000)
    ymax = min(280000, max(todos) + 2000)
    if ymax > ymin:
        ax.set_ylim(ymin, ymax)

    return line_ir, line_red

ani = animation.FuncAnimation(fig, atualizar, interval=40, blit=False, cache_frame_data=False)
plt.tight_layout()
plt.show()

ser.close()
