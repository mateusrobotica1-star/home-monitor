-- Tabela de configuração/estado (ex: buzzer ligado/desligado)
CREATE TABLE IF NOT EXISTS config (
    chave TEXT PRIMARY KEY,
    valor INTEGER DEFAULT 0,
    atualizado_em TIMESTAMPTZ DEFAULT now()
);

-- Insere o estado inicial do buzzer (desligado)
INSERT INTO config (chave, valor) VALUES ('buzzer', 0)
ON CONFLICT (chave) DO NOTHING;