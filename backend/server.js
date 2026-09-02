// ========================================
// BACKEND - API para monitor de temperatura
// Hospedar na Render.com
// ========================================

require('dotenv').config();
const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const morgan = require('morgan');
const jwt = require('jsonwebtoken');
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

// ====================================
// AUTENTICAÇÃO DO SITE (login)
// ====================================
// Usuário e senha do site (configuráveis via variáveis de ambiente)
const SITE_USER = process.env.SITE_USER || 'mateus';
const SITE_PASS = process.env.SITE_PASS || 'mateus123';

// Segredo para assinar os tokens (use uma string aleatória forte)
const JWT_SECRET = process.env.JWT_SECRET || (API_KEY + '|' + SITE_USER + SITE_PASS);

// Middleware de autenticação para o ESP32
function autenticar(req, res, next) {
  const chave = req.headers['x-api-key'];
  if (chave === API_KEY) {
    next();
  } else {
    res.status(401).json({ erro: 'Chave de API inválida' });
  }
}

// Middleware para exigir token JWT válido (site)
function exigirToken(req, res, next) {
  const header = req.headers['authorization'];
  if (!header || !header.startsWith('Bearer ')) {
    return res.status(401).json({ erro: 'Token não fornecido' });
  }
  const token = header.slice(7);
  try {
    const payload = jwt.verify(token, JWT_SECRET);
    req.usuario = payload.usuario;
    next();
  } catch (err) {
    return res.status(401).json({ erro: 'Token inválido ou expirado' });
  }
}

// ========================================
// ROTA: Login do site
// POST /api/login
// ========================================
app.post('/api/login', (req, res) => {
  const { usuario, senha } = req.body;

  if (usuario === SITE_USER && senha === SITE_PASS) {
    const token = jwt.sign({ usuario }, JWT_SECRET, { expiresIn: '7d' });
    res.json({ token, usuario });
  } else {
    res.status(401).json({ erro: 'Usuário ou senha incorretos' });
  }
});

// ========================================
// ROTA: Receber dados do ESP32
// POST /api/temperatura
// ========================================
app.post('/api/temperatura', autenticar, async (req, res) => {
  const { temperatura, umidade, som } = req.body;

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
          som: som !== undefined ? parseInt(som) : 0,
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
app.get('/api/leituras', exigirToken, async (req, res) => {
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
app.get('/api/temperatura/atual', exigirToken, async (req, res) => {
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
      { metodo: 'POST', caminho: '/api/temperatura', descricao: 'Recebe dados do ESP32 (x-api-key)' },
      { metodo: 'POST', caminho: '/api/login', descricao: 'Login do site - retorna token' },
      { metodo: 'GET', caminho: '/api/leituras?limite=10', descricao: 'Busca últimas leituras (Bearer token)' },
      { metodo: 'GET', caminho: '/api/temperatura/atual', descricao: 'Última leitura registrada (Bearer token)' },
      { metodo: 'GET', caminho: '/health', descricao: 'Verifica se está online' }
    ]
  });
});

app.listen(PORT, () => {
  console.log(`Servidor rodando na porta ${PORT}`);
});
