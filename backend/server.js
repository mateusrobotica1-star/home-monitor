// ========================================
// BACKEND - API para monitor de temperatura
// Hospedar na Render.com
// ========================================

require('dotenv').config();
const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const morgan = require('morgan');
const { createClient } = require('@supabase/supabase-js');

const app = express();
const PORT = process.env.PORT || 3000;

// ====================================
// CONFIGURAÇÃO DO SUPABASE
// ====================================
const supabaseUrl = process.env.SUPABASE_URL;
const supabaseKey = process.env.SUPABASE_SERVICE_KEY;

if (!supabaseUrl || !supabaseKey) {
  console.error('ERRO: Defina SUPABASE_URL e SUPABASE_SERVICE_KEY');
  process.exit(1);
}

const supabase = createClient(supabaseUrl, supabaseKey);

// Middlewares
app.use(helmet());
app.use(cors());
app.use(morgan('combined'));
app.use(express.json());

// Chave secreta para autenticar o ESP32
const API_KEY = process.env.API_KEY;

// Middleware de autenticação para o ESP32
function autenticar(req, res, next) {
  const chave = req.headers['x-api-key'];
  if (chave === API_KEY) {
    next();
  } else {
    res.status(401).json({ erro: 'Chave de API inválida' });
  }
}

// ========================================
// ROTA: Receber dados do ESP32
// POST /api/temperatura
// ========================================
app.post('/api/temperatura', autenticar, async (req, res) => {
  const { temperatura, umidade } = req.body;

  if (temperatura === undefined || umidade === undefined) {
    return res.status(400).json({ erro: 'Campos temperatura e umidade são obrigatórios' });
  }

  try {
    const { data, error } = await supabase
      .from('leituras')
      .insert([
        {
          temperatura: parseFloat(temperatura),
          umidade: parseFloat(umidade),
          local: 'quarto',
          criado_em: new Date().toISOString()
        }
      ]);

    if (error) throw error;

    res.status(201).json({ mensagem: 'Leitura registrada com sucesso', data });
  } catch (error) {
    console.error('Erro ao salvar:', error.message);
    res.status(500).json({ erro: 'Erro ao salvar a leitura' });
  }
});

// ========================================
// ROTA: Buscar últimas leituras
// GET /api/leituras?limite=10
// ========================================
app.get('/api/leituras', async (req, res) => {
  const limite = parseInt(req.query.limite) || 24;

  try {
    const { data, error } = await supabase
      .from('leituras')
      .select('*')
      .order('criado_em', { ascending: false })
      .limit(limite);

    if (error) throw error;

    res.json(data);
  } catch (error) {
    console.error('Erro ao buscar:', error.message);
    res.status(500).json({ erro: 'Erro ao buscar leituras' });
  }
});

// ========================================
// ROTA: Última leitura atual
// GET /api/temperatura/atual
// ========================================
app.get('/api/temperatura/atual', async (req, res) => {
  try {
    const { data, error } = await supabase
      .from('leituras')
      .select('*')
      .order('criado_em', { ascending: false })
      .limit(1);

    if (error) throw error;

    if (data.length === 0) {
      return res.status(404).json({ mensagem: 'Nenhuma leitura encontrada' });
    }

    res.json(data[0]);
  } catch (error) {
    console.error('Erro ao buscar:', error.message);
    res.status(500).json({ erro: 'Erro ao buscar leitura' });
  }
});

// ========================================
// ROTA: Health check
// GET /health
// ========================================
app.get('/health', (req, res) => {
  const now = new Date().toISOString();
  res.json({ status: 'ok', servidor: 'Monitor Casa', horario: now });
});

// Rota principal (raiz)
app.get('/', (req, res) => {
  res.json({
    nome: 'Monitor de Temperatura da Casa API',
    endpoints: [
      { metodo: 'POST', caminho: '/api/temperatura', descricao: 'Recebe dados do ESP32' },
      { metodo: 'GET', caminho: '/api/leituras?limite=10', descricao: 'Busca últimas leituras' },
      { metodo: 'GET', caminho: '/api/temperatura/atual', descricao: 'Última leitura registrada' },
      { metodo: 'GET', caminho: '/health', descricao: 'Verifica se está online' }
    ]
  });
});

app.listen(PORT, () => {
  console.log(`Servidor rodando na porta ${PORT}`);
});
