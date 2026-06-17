#ifndef MQTT_CERTS_H
#define MQTT_CERTS_H

// ─── Certificado CA do broker (obrigatório para TLS) ─────────────────────────
//
// Como obter:
//   HiveMQ Cloud  → baixe em: Account → Cluster → TLS → Download Certificate
//   Mosquitto     → copie o arquivo ca.crt gerado na configuração do broker
//   AWS IoT       → console → Security → CAs → Download
//
// Cole o conteúdo PEM abaixo (incluindo os delimitadores BEGIN/END):

#define MQTT_CA_CERT                                        \
    "-----BEGIN CERTIFICATE-----\n"                         \
    "SUBSTITUA PELO CERTIFICADO CA DO SEU BROKER AQUI\n"    \
    "-----END CERTIFICATE-----\n"

// ─── Autenticação mútua TLS (mTLS) — opcional ────────────────────────────────
//
// Deixe NULL se o broker não exigir certificado do cliente.
// Se usar mTLS, preencha os dois abaixo:

#define MQTT_CLIENT_CERT    NULL

// "-----BEGIN CERTIFICATE-----\n"
// "... certificado do cliente ...\n"
// "-----END CERTIFICATE-----\n"

#define MQTT_CLIENT_KEY     NULL

// "-----BEGIN RSA PRIVATE KEY-----\n"
// "... chave privada do cliente ...\n"
// "-----END RSA PRIVATE KEY-----\n"

#endif
