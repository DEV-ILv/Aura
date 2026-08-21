-- =============================================================================
-- AURA Companion — grant table access to PostgREST roles (incremental fix)
-- =============================================================================
-- Applies to an ALREADY-CREATED schema. When the base migration
-- (20260803000001_init_schema.sql) recreated the tables, the privileges that
-- Supabase normally auto-grants were dropped along with the tables, so
-- PostgREST returned `42501 permission denied for table ...` to both the
-- anon and authenticated roles.
--
-- Run this once from the Supabase SQL editor:
--
--   1. Open https://<project-ref>.supabase.co → SQL Editor
--   2. Paste the contents of this file
--   3. Run
--
-- GRANT is idempotent, so this is safe to re-run. RLS is unaffected — these
-- grants only let requests through so RLS policies can evaluate them.
-- =============================================================================

grant all on table public.users         to anon, authenticated, service_role;
grant all on table public.devices       to anon, authenticated, service_role;
grant all on table public.commands      to anon, authenticated, service_role;
grant all on table public.reminders     to anon, authenticated, service_role;
grant all on table public.memory        to anon, authenticated, service_role;
grant all on table public.notifications to anon, authenticated, service_role;
grant all on table public.settings      to anon, authenticated, service_role;
