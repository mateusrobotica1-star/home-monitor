-- ========================================
-- SQL para criar a tabela no Supabase
-- Execute no SQL Editor do Supabase
-- ========================================

-- Cria a tabela de leituras
CREATE TABLE leituras (
    id BIGSERIAL PRIMARY KEY,
    temperatura DOUBLE PRECISION NOT NULL,
    umidade DOUBLE PRECISION NOT NULL,
    local TEXT DEFAULT 'quarto',
    criado_em TIMESTAMPTZ DEFAULT NOW()
);

-- Cria um índice para melhorar a performance de buscas por data
CREATE INDEX idx_leituras_criado_em ON leituras (criado_em DESC);

-- Cria a função para obter a média de temperatura das últimas 24 horas
CREATE OR REPLACE FUNCTION media_temperatura_24h()
RETURNS DOUBLE PRECISION AS $$
BEGIN
    RETURN (
        SELECT AVG(temperatura)
        FROM leituras
        WHERE criado_em >= NOW() - INTERVAL '24 hours'
    );
END;
$$ LANGUAGE plpgsql;

-- Cria a função para obter a temperatura máxima das últimas 24 horas
CREATE OR REPLACE FUNCTION temperatura_maxima_24h()
RETURNS DOUBLE PRECISION AS $$
BEGIN
    RETURN (
        SELECT MAX(temperatura)
        FROM leituras
        WHERE criado_em >= NOW() - INTERVAL '24 hours'
    );
END;
$$ LANGUAGE plpgsql;

-- Cria a função para obter a temperatura mínima das últimas 24 horas
CREATE OR REPLACE FUNCTION temperatura_minima_24h()
RETURNS DOUBLE PRECISION AS $$
BEGIN
    RETURN (
        SELECT MIN(temperatura)
        FROM leituras
        WHERE criado_em >= NOW() - INTERVAL '24 hours'
    );
END;
$$ LANGUAGE plpgsql;
