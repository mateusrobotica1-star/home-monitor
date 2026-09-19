-- ========================================
-- SQL: Adicionar coluna de movimento na tabela leituras
-- Execute no SQL Editor do Supabase
-- ========================================

-- Adiciona a coluna 'movimento' (true = pessoa/objeto detectado), padrão false
ALTER TABLE leituras
ADD COLUMN IF NOT EXISTS movimento BOOLEAN DEFAULT false;
