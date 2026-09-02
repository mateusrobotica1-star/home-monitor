# 🏠 Monitor de Temperatura da Casa

Projeto completo de IoT para monitorar a temperatura do seu quarto usando:
- **ESP32-S3** + sensor **DHT11**
- **Supabase** (banco de dados)
- **Render.com** (hospedagem do site e da API)

## 📁 Estrutura do projeto

```
home-monitor/
├── esp32-code/
│   └── esp32_dht11.ino     # Código Arduino para ESP32-S3
├── backend/
│   ├── server.js           # API Node.js/Express
│   ├── package.json
│   └── .env.example        # Modelo das variáveis de ambiente
├── frontend/
│   ├── server.js           # Servidor do dashboard
│   └── public/
│       └── index.html      # Dashboard visual
├── supabase.sql            # Script SQL do banco de dados
└── render.yaml             # Config da Render (opcional)
```

## 🚀 Passo a passo para configurar

### Passo 1: Configurar o Supabase (banco de dados)

1. Acesse [supabase.com](https://supabase.com) e crie uma conta gratuita
2. Clique em **New project** e crie um projeto (grátis até 500MB)
3. Aguarde o setup terminar (1-2 minutos)
4. No menu à esquerda, clique em **SQL Editor**
5. Cole o conteúdo do arquivo `supabase.sql` e clique em **Run**
6. No menu à esquerda, vá em **Settings → API**
7. Anote:
   - **Project URL** (ex: `https://abc123.supabase.co`)
   - **service_role secret** (senha longa, mantenha em segredo!)

### Passo 2: Configurar o backend (API)

1. Crie uma conta no [render.com](https://render.com) (grátis)
2. Clique em **New + → Blueprint** ou **New + → Web Service**
3. Conecte seu repositório GitHub (ou faça deploy manual)

Se usar **Web Service** (mais simples):
- **Name:** `monitor-backend`
- **Root Directory:** `backend`
- **Environment:** `Node`
- **Build Command:** `npm install`
- **Start Command:** `npm start`

Em **Environment Variables**, adicione:
| Variável | Valor |
|----------|-------|
| `SUPABASE_URL` | A URL do seu projeto Supabase |
| `SUPABASE_SERVICE_KEY` | A chave service_role do Supabase |
| `API_KEY` | Uma senha secreta que você inventar (ex: `minha-casa-secreta-123`) |
| `PORT` | `3000` |

4. Clique em **Create Web Service**
5. Aguarde o deploy (2-3 minutos)
6. Anote a URL gerada (ex: `https://monitor-backend.onrender.com`)

> 💡 Dica: No plano grátis, a Render "dorme" após 15 min de inatividade. O ESP32 enviando a cada minuto mantém o serviço acordado!

### Passo 3: Configurar o ESP32-S3

1. Abra a **Arduino IDE**
2. Vá em **Arquivo → Preferências** e na aba "URLs Adicionais para Gerenciadores de Placas" adicione:
   ```
   https://dl.espressif.com/dl/package_esp32_index.json
   ```
3. No **Gerenciador de Placas** (Tools → Board → Boards Manager), instale **esp32** (Espressif)
4. Instale a biblioteca DHT:
   - **Sketch → Include Library → Manage Libraries**
   - Pesquise **DHT sensor library** (Adafruit) e instale
5. Conecte o DHT11 no ESP32-S3:
   - **VCC** → **3.3V** (ou pin 5V do ESP32-S3)
   - **GND** → **GND**
   - **DATA** → **GPIO4** (definido no código)
6. Abra `esp32-code/esp32_dht11.ino` e ALTERE:
   - `WIFI_SSID` → seu WiFi
   - `WIFI_PASSWORD` → senha do WiFi
   - `API_URL` → a URL do seu backend na Render + `/api/temperatura`
   - `API_KEY` → a MESMA chave que definiu na Render
7. Selecione a placa **ESP32S3 Dev Module** em Tools → Board
8. Clique em **Upload** para gravar no ESP32

### Passo 4: Subir o site (frontend)

1. No **render.com**, clique em **New + → Web Service**
2. **Name:** `monitor-frontend`
3. **Root Directory:** `frontend`
4. **Environment:** `Node`
5. **Build Command:** `npm install`
6. **Start Command:** `npm start`
7. Clique em **Create Web Service**
8. Após o deploy, abra o arquivo `frontend/public/index.html` e altere:
   - `API_URL` → a URL do seu backend (ex: `https://monitor-backend.onrender.com/api`)
9. Faça o upload da alteração para o GitHub e a Render fará o redeploy automaticamente

### Passo 5: Acessar o site

- Acesse a URL do seu frontend (ex: `https://monitor-frontend.onrender.com`)
- Você verá a temperatura, umidade e o gráfico histórico!

## 🔌 Conexão do DHT11

```
        DHT11
      ┌───────┐
      │  [1]  │→ VCC → 3.3V / 5V (ESP32-S3)
      │  [2]  │→ DATA → GPIO4
      │  [3]  │→ (sem conexão)
      │  [4]  │→ GND → GND
      └───────┘
```

> 💡 Dica: Adicione um resistor de 10kΩ entre DATA e VCC (pull-up)

## 📊 Endpoints da API

| Método | Endpoint | Descrição |
|--------|----------|-----------|
| POST | `/api/temperatura` | Recebe dados do ESP32 (requer `x-api-key`) |
| GET | `/api/leituras?limite=24` | Últimas leituras |
| GET | `/api/temperatura/atual` | Última leitura registrada |
| GET | `/health` | Verifica se a API está online |

## ⚠️ Dicas de segurança

- **NUNCA** exponha a chave `service_role` do Supabase em código público
- A chave `API_KEY` no ESP32 vai viajando pela internet, então use um valor complexo
- O dashboard é público por padrão - se quiser proteção, adicione login depois

## 🛠️ Problemas comuns

**O ESP32 não conecta no WiFi?**
- Verifique se SSID e senha estão corretos
- Confirme que o ESP32-S3 está no modo WiFi

**A API não responde?**
- Confira se as variáveis de ambiente estão configuradas corretamente na Render
- Teste no navegador: `https://SEU-BACKEND.onrender.com/health`

**Dados não aparecem no dashboard?**
- Confirme que o ESP32 está enviando (abra o Serial Monitor)
- Verifique se `API_URL` no dashboard aponta para o backend correto
