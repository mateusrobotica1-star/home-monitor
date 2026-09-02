// ========================================
// FRONTEND - Dashboard de Monitoramento
// Hospedar também na Render.com
// ========================================

const express = require('express');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;

// Servir arquivos estáticos da pasta public
app.use(express.static(path.join(__dirname, 'public')));

app.listen(PORT, () => {
  console.log(`Dashboard rodando na porta ${PORT}`);
});
