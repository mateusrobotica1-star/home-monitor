-- ========================================
-- SQL: Adicionar coluna de som na tabela leituras
-- Execute no SQL Editor do Supabase
-- ========================================

-- Adiciona a coluna 'som' (nível de intensidade sonora), padrão 0 para leituras antigas
ALTER TABLE leituras
ADD COLUMN IF NOT EXISTS som INTEGER DEFAULT 0;