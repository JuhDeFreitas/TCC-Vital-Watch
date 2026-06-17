#ifndef ALERT_MANAGER_H
#define ALERT_MANAGER_H

// Estado de atividade do paciente — atualizado pelo MPU6050
typedef enum {
    PATIENT_RESTING,
    PATIENT_RUNNING,
} patient_state_t;

// Estado atual do paciente (acessível por outros módulos se necessário)
extern volatile patient_state_t g_patient_state;

// Atualiza o estado de atividade. Chamado pelo on_motion em main.c.
void alert_manager_set_state(patient_state_t state);

// Verifica BPM e SpO2 contra os limiares do estado atual.
// Publica alerta via MQTT se os valores estiverem fora do normal.
// Chamado a cada nova leitura do sensor (dentro do on_vitals).
void alert_manager_check(int bpm, double spo2);

#endif
