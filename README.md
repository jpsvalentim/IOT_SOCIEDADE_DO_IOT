# 🌍 SISTEMA IOT PARA MONITORAMENTO DE ENCOSTAS E PREVENÇÃO DE DESLIZAMENTOS

> Projeto interdisciplinar desenvolvido no SENAI - Jaraguá do Sul (CENTROWEG), integrando sensores ambientais com tecnologia IoT  para apoiar ações de prevenção a deslizamentos de terra.

---

## 👨‍💻 Autores

- **Julia Gabrieli Garcia da Silva**  
- **Raquel da Silva**  
- **Eduarda Schwirkowski Dalsochio**

## 👨‍💻 Orientador
- **João Pedro Silva Valentim**

**Instituição:** SENAI - Jaraguá do Sul - CENTROWEG  
**Curso:** Aprendizagem Industrial em Programador de Sistemas da Informação / Técnico em Manutenção Eletroeletrônico  
**Período:** 3º semestre / 2025  
**Área:** Ciências Exatas e da Terra – Ciência da Computação

---

## 📦 Sobre o Projeto

Este projeto visa desenvolver um sistema de monitoramento ambiental baseado em **Internet das Coisas (IoT)**, focado na coleta e análise de dados relevantes à **prevenção de deslizamentos em encostas**.

Os dispositivos coletam dados como:
- Temperatura e umidade do ar (DHT22)
- Luminosidade (LDR)
- Umidade do solo
- Inclinação e vibração do terreno (MPU6050, sensor de vibração)

Essas informações são enviadas via **Wi-Fi (MQTT)** para uma plataforma de visualização e análise em tempo real (**Node-RED** e banco de dados MySQL).

---

## ⚙️ Tecnologias e Ferramentas

| Tecnologia | Finalidade |
|------------|------------|
| **ESP32** + PlatformIO (VS Code) | Microcontrolador principal |
| **C++** (Arduino Framework) | Lógica de programação embarcada |
| **MQTT** (HiveMQ / Mosquitto / Azure) | Protocolo leve de envio de dados |
| **Node-RED** | Visualização dos dados e emissão de alertas |
| **MySQL / Railway** | Armazenamento em nuvem |
| **VS Code + GitHub** | Versionamento e desenvolvimento colaborativo |

---

## 🗂️ Estrutura do Projeto

```plaintext
/
├── include/           # Headers personalizados
├── lib/               # Bibliotecas auxiliares (locais)
├── src/               # Código-fonte principal (main.cpp)
├── test/              # Casos de teste (opcional)
├── platformio.ini     # Configurações do PlatformIO
├── README.md          # Este arquivo :)
