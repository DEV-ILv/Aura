-- =============================================================================
-- AURA Companion — default auth.uid() on owner columns (incremental fix)
-- =============================================================================
-- Found during live verification (2026-08-04):
--
-- The companion app inserts rows WITHOUT a `user_id` column (see
-- lib/repositories/cloud_repository.dart — the client deliberately relies on
-- RLS to scope data). The base migration defined the owner columns as
-- `uuid not null` with NO default, so every INSERT was rejected with:
--
--     42501 new row violates row-level security policy for table "devices"
--
-- because `WITH CHECK (auth.uid() = user_id)` evaluated `auth.uid() = NULL`.
-- Cloud mode therefore could not register devices, log commands, create
-- reminders, save memories, log notifications, or push settings.
--
-- This migration adds `default auth.uid()` to every owner column so RLS fills
-- the owner from the session. Security is unchanged: the RLS policies still
-- enforce that a row's owner must equal the caller, so a client cannot set
-- someone else's user_id (that path returns 403).
--
-- Run once from the Supabase SQL editor (idempotent, safe to re-run):
--
--   1. Open https://<project-ref>.supabase.co → SQL Editor
--   2. Paste the contents of this file
--   3. Run
--
-- For fresh installs these defaults are already part of
-- 20260803000001_init_schema.sql, so this file is only needed on databases
-- that applied the earlier migration.
-- =============================================================================

alter table public.users         alter column id       set default auth.uid();
alter table public.devices       alter column user_id  set default auth.uid();
alter table public.commands      alter column user_id  set default auth.uid();
alter table public.reminders     alter column user_id  set default auth.uid();
alter table public.memory        alter column user_id  set default auth.uid();
alter table public.notifications alter column user_id  set default auth.uid();
alter table public.settings      alter column user_id  set default auth.uid();
